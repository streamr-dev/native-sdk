// Module streamr.dht.Endpoint
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/Endpoint.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;
#include <new>


export module streamr.dht.Endpoint;

import std;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;
import streamr.utils.EnableSharedFromThis;
import streamr.dht.ConnectedEndpointState;
import streamr.dht.ConnectingEndpointState;
import streamr.dht.DisconnectedEndpointState;
import streamr.dht.EndpointState;
import streamr.dht.EndpointStateInterface;
import streamr.dht.InitialEndpointState;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;
export namespace streamr::dht::connection::endpoint {

using ::dht::PeerDescriptor;

namespace endpointevents {

struct Connected : Event<> {};
struct Disconnected : Event<> {};
struct Data : Event<std::vector<std::byte>> {};

} // namespace endpointevents

using EndpointEvents = std::tuple<
    endpointevents::Data,
    endpointevents::Connected,
    endpointevents::Disconnected>;

// Endpoint implements EndpointStateInterface (privately) so the state
// classes can drive the machine through the abstract interface — see
// EndpointStateInterface.cppm for why this replaces the former concrete
// forwarder member, and for the locking contract this class implements
// (phase A0): one mutex guards the whole state machine, transitions
// happen under it, and every call-out that can emit an event runs after
// it is released. Each public method is therefore two-phase: a locked
// section that asks the current state to transition (collecting any
// deferred call-out it returns), then the call-outs.
class Endpoint : public EventEmitter<EndpointEvents>,
                 public EnableSharedFromThis,
                 private EndpointStateInterface {
private:
    std::function<void()> removeSelfFromContainer;
    EndpointState* state;
    // The single state-machine mutex; guards the current-state pointer
    // and all state-owned resources. Recursive because the states drive
    // nested transitions through EndpointStateInterface while the public
    // entry point already holds it.
    std::recursive_mutex mutex;
    // Highest pending-connection sequence number adopted so far; guarded
    // by mutex. See setConnecting().
    std::uint64_t lastConnectingSequence = 0;

    // --- EndpointStateInterface (called by the state classes) ---

    std::recursive_mutex& getStateMachineMutex() override {
        return this->mutex;
    }

    void enterConnectingState(
        const std::shared_ptr<IPendingConnection>& pendingConnection) override {
        SLogger::debug("Endpoint::enterConnectingState");
        this->state = this->connectingState.get();
        this->connectingState->adoptPendingConnection(pendingConnection);
    }

    void enterConnectedState(
        const std::shared_ptr<Connection>& connection) override {
        SLogger::debug("Endpoint::enterConnectedState");
        this->state = this->connectedState.get();
        this->connectedState->enterState(connection, this->weakPin());
    }

    bool enterDisconnectedState() override {
        SLogger::debug("Endpoint::enterDisconnectedState");
        if (this->state == this->disconnectedState.get()) {
            return false;
        }
        this->state = this->disconnectedState.get();
        return true;
    }

    void emitData(const std::vector<std::byte>& data) override {
        SLogger::debug("Endpoint::emitData");
        this->emit<endpointevents::Data>(data);
    }

    void emitConnected() override {
        SLogger::debug("Endpoint::emitConnected");
        this->emit<endpointevents::Connected>();
    }

    void finishDisconnect() override {
        SLogger::debug("Endpoint::finishDisconnect start");
        // Container removal (and the container's own Disconnected
        // notification) must complete before the endpoint-level emit:
        // ConnectionManager::gracefullyDisconnect() resumes on the
        // endpoint-level event and stop() then removes all transport
        // listeners — the reverse order loses the transport-level
        // Disconnected on the stopping side (seen as the disconnected3
        // timeout in the ConnectionManagerIntegrationTest stress runs).
        this->removeSelfFromContainer();
        this->emit<endpointevents::Disconnected>();
        this->removeAllListeners();
        SLogger::debug("Endpoint::finishDisconnect end");
    }

    // Type-erased weak reference the state handlers pin before touching
    // the machine, so a late connection event cannot race the Endpoint's
    // destruction (the states only hold a plain reference back here).
    [[nodiscard]] std::weak_ptr<void> weakPin() {
        return this->sharedFromThis<Endpoint>();
    }

    std::shared_ptr<InitialEndpointState> initialState;
    std::shared_ptr<ConnectedEndpointState> connectedState;
    std::shared_ptr<ConnectingEndpointState> connectingState;
    std::shared_ptr<DisconnectedEndpointState> disconnectedState;
    PeerDescriptor peerDescriptor;

    explicit Endpoint(
        PeerDescriptor peerDescriptor,
        std::function<void()>&& removeSelfFromContainer)
        : connectedState(ConnectedEndpointState::newInstance(*this)),
          connectingState(ConnectingEndpointState::newInstance(*this)),
          disconnectedState(DisconnectedEndpointState::newInstance(*this)),
          initialState(InitialEndpointState::newInstance(*this)),
          peerDescriptor(std::move(peerDescriptor)),
          removeSelfFromContainer(removeSelfFromContainer) {
        SLogger::debug("Endpoint constructor start");
        this->state = this->initialState.get();
        SLogger::debug("Endpoint constructor end");
    }

public:
    [[nodiscard]] static std::shared_ptr<Endpoint> newInstance(
        PeerDescriptor peerDescriptor,
        std::function<void()>&& removeSelfFromContainer) {
        struct MakeSharedEnabler : public Endpoint {
            explicit MakeSharedEnabler(
                PeerDescriptor peerDescriptor,
                std::function<void()>&& removeSelfFromContainer)
                : Endpoint(
                      std::move(peerDescriptor),
                      std::move(removeSelfFromContainer)) {}
        };
        return std::make_shared<MakeSharedEnabler>(
            std::move(peerDescriptor), std::move(removeSelfFromContainer));
    }

    ~Endpoint() override { SLogger::debug("Endpoint destructor"); }

    // sequenceNumber orders the pending connections handed to this
    // endpoint. ConnectionManager assigns it under endpointsMutex, so it
    // reflects the order in which the tie-break DECIDED, which can differ
    // from the order in which these setConnecting() calls actually run
    // (the initial adoption from send() on the main thread races the
    // replacing adoption from an incoming connection on the connector's
    // dispatcher thread). Adopting only strictly-newer sequence numbers
    // makes the outcome independent of that thread race, so both peers
    // still converge on the connection the tie-break chose.
    void setConnecting(
        const std::shared_ptr<IPendingConnection>& pendingConnection,
        std::uint64_t sequenceNumber) {
        SLogger::debug("Endpoint::setConnecting start");
        auto self = this->sharedFromThis<Endpoint>();
        std::function<void()> callout;
        bool adopted = false;
        {
            std::scoped_lock lock(this->mutex);
            if (sequenceNumber <= this->lastConnectingSequence) {
                SLogger::debug(
                    "Endpoint::setConnecting ignoring superseded pending"
                    " connection");
                return;
            }
            this->lastConnectingSequence = sequenceNumber;
            callout = this->state->changeToConnectingState(pendingConnection);
            adopted = this->state == this->connectingState.get() &&
                this->connectingState->isCurrentPendingConnection(
                    pendingConnection);
        }
        if (callout) {
            callout();
        }
        if (adopted) {
            // Registering on the (replaying) pending connection may
            // complete the connection synchronously, so it must happen
            // outside the mutex — see
            // ConnectingEndpointState::registerEventHandlers.
            this->connectingState->registerEventHandlers(
                pendingConnection, self);
        }
        SLogger::debug("Endpoint::setConnecting end");
    }

    void setConnected(const std::shared_ptr<Connection>& connection) {
        SLogger::debug("Endpoint::setConnected start");
        auto self = this->sharedFromThis<Endpoint>();
        std::function<void()> callout;
        {
            std::scoped_lock lock(this->mutex);
            callout = this->state->changeToConnectedState(connection);
        }
        if (callout) {
            callout();
        }
        SLogger::debug("Endpoint::setConnected end");
    }

    [[nodiscard]] bool isConnected() {
        SLogger::debug("Endpoint::isConnected");
        std::scoped_lock lock(this->mutex);
        return this->state->isConnected();
    }

    [[nodiscard]] PeerDescriptor getPeerDescriptor() const {
        SLogger::debug("Endpoint::getPeerDescriptor");
        // Immutable after construction, so no lock is needed — callers
        // may hold container locks.
        return this->peerDescriptor;
    }

    void close(bool graceful) {
        SLogger::debug("Endpoint::close start");
        auto self = this->sharedFromThis<Endpoint>();
        std::function<void()> callout;
        bool transitioned = false;
        {
            std::scoped_lock lock(this->mutex);
            callout = this->state->close(graceful);
            transitioned = this->enterDisconnectedState();
        }
        if (callout) {
            callout();
        }
        if (transitioned) {
            this->finishDisconnect();
        }
        SLogger::debug("Endpoint::close end");
    }

    void send(const std::vector<std::byte>& data) {
        SLogger::debug("Endpoint::send start");
        auto self = this->sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        // ConnectedEndpointState forwards to connection->send() under
        // the mutex: a deliberate exception to the no-call-outs rule —
        // it emits nothing, and holding the mutex keeps direct sends
        // ordered with the connecting-state buffer flush.
        this->state->send(data);
        SLogger::debug("Endpoint::send end");
    }
};

} // namespace streamr::dht::connection::endpoint
