// Module streamr.dht.ConnectingEndpointState
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/ConnectingEndpointState.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;
#include <new>


export module streamr.dht.ConnectingEndpointState;

import std;

import streamr.logger.SLogger;
import streamr.dht.Connection;
import streamr.dht.IPendingConnection;
import streamr.eventemitter.EventEmitter;
import streamr.dht.EndpointState;
import streamr.dht.EndpointStateInterface;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::eventemitter::HandlerToken;
// Self-sufficient shorthand (was inherited textually from a
// neighboring header before consolidation).
using streamr::logger::SLogger;

export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::IPendingConnection;

// The state's resources (pending connection, tokens, send buffer) are
// guarded by the Endpoint's state-machine mutex (phase A0: the former
// per-state mutex formed an ABBA pair with the endpoint mutex between a
// sending thread and the connection-event dispatcher thread). Methods
// overridden from EndpointState are called with that mutex held; the
// event handlers registered on the pending connection take it
// themselves and first validate that they still belong to the current
// pending connection — off() cannot stop a handler that an in-flight
// emit has already started, and the ReplayEventEmitter can replay a
// stored event during registration, so token bookkeeping alone is not
// enough.
class ConnectingEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;
    std::vector<std::byte> buffer;
    std::shared_ptr<IPendingConnection> pendingConnection;
    HandlerToken connectedHandlerToken;
    HandlerToken disconnectedHandlerToken;

    explicit ConnectingEndpointState(EndpointStateInterface& stateInterface)
        : stateInterface(stateInterface) {
        SLogger::debug("ConnectingEndpointState constructor");
    }

    // Requires the state-machine mutex. Detaches the handlers from the
    // current pending connection and forgets it; a handler already
    // running past its off() no-ops on the identity check below.
    // Default (id 0) tokens are not off()'d: besides being no-ops, the
    // calls would take the pending connection's handler mutex, and while
    // the tokens are still uncommitted a registerEventHandlers() replay
    // on another thread can be running under that same mutex while
    // waiting for the state-machine mutex — off() here would complete
    // the ABBA cycle.
    void detachFromPendingConnection() {
        SLogger::debug("ConnectingEndpointState::detachFromPendingConnection");
        if (this->pendingConnection) {
            if (this->connectedHandlerToken.getId() != 0) {
                this->pendingConnection
                    ->off<pendingconnectionevents::Connected>(
                        this->connectedHandlerToken);
            }
            if (this->disconnectedHandlerToken.getId() != 0) {
                this->pendingConnection
                    ->off<pendingconnectionevents::Disconnected>(
                        this->disconnectedHandlerToken);
            }
            this->connectedHandlerToken = HandlerToken{};
            this->disconnectedHandlerToken = HandlerToken{};
            this->pendingConnection.reset();
        }
    }

public:
    [[nodiscard]] static std::shared_ptr<ConnectingEndpointState> newInstance(
        EndpointStateInterface& stateInterface) {
        struct MakeSharedEnabler : public ConnectingEndpointState {
            explicit MakeSharedEnabler(EndpointStateInterface& stateInterface)
                : ConnectingEndpointState(stateInterface) {}
        };
        return std::make_shared<MakeSharedEnabler>(stateInterface);
    }

    ~ConnectingEndpointState() override {
        SLogger::debug("ConnectingEndpointState destructor");
    }

    // Requires the state-machine mutex (called by
    // Endpoint::enterConnectingState). Replaces the current pending
    // connection; the buffer is kept — data buffered while waiting for a
    // pending connection that lost the simultaneous-connect tiebreak
    // must still go out once its replacement connects.
    void adoptPendingConnection(
        const std::shared_ptr<IPendingConnection>& pendingConnection) {
        SLogger::debug("ConnectingEndpointState::adoptPendingConnection");
        this->detachFromPendingConnection();
        this->pendingConnection = pendingConnection;
    }

    // Requires the state-machine mutex. Endpoint uses this to decide
    // whether the pending connection it adopted is still the current one
    // and therefore needs its handlers registered.
    [[nodiscard]] bool isCurrentPendingConnection(
        const std::shared_ptr<IPendingConnection>& pendingConnection) const {
        return this->pendingConnection == pendingConnection;
    }

    // Must be called WITHOUT the state-machine mutex: the pending
    // connection is a ReplayEventEmitter, so on() can invoke the handler
    // synchronously (replaying an event that arrived before registration
    // — the PR #43 race), and the handler's own call-outs must not run
    // under the mutex. Registration is therefore a separate step after
    // the transition instead of part of it; the replay semantics
    // guarantee nothing is lost in between. The tokens are committed
    // only if this pending connection is still the current one — a
    // replayed event or a concurrent transition may have moved the
    // machine on, in which case the handlers are taken right back off.
    void registerEventHandlers(
        const std::shared_ptr<IPendingConnection>& pendingConnection,
        const std::weak_ptr<void>& endpointPin) {
        SLogger::debug("ConnectingEndpointState::registerEventHandlers start");
        std::weak_ptr<ConnectingEndpointState> weakSelf =
            this->sharedFromThis<ConnectingEndpointState>();
        auto* rawPendingConnection = pendingConnection.get();
        // Disconnected is registered first: if the replay store holds
        // both a Connected and a Disconnected event, the disconnect wins
        // (the identity check makes the later Connected replay a no-op),
        // matching the connection's real fate.
        auto disconnectedToken =
            pendingConnection->on<pendingconnectionevents::Disconnected>(
                [weakSelf, endpointPin, rawPendingConnection](
                    bool /*gracefulLeave*/) {
                    // The pin keeps the Endpoint (and with it this
                    // state) alive for the duration of the handler.
                    const auto pin = endpointPin.lock();
                    if (!pin) {
                        return;
                    }
                    const auto self = weakSelf.lock();
                    if (!self) {
                        return;
                    }
                    bool transitioned = false;
                    {
                        std::scoped_lock lock(
                            self->stateInterface.getStateMachineMutex());
                        if (self->pendingConnection.get() !=
                            rawPendingConnection) {
                            return; // stale handler of a replaced pending
                                    // connection
                        }
                        self->detachFromPendingConnection();
                        self->buffer.clear();
                        transitioned =
                            self->stateInterface.enterDisconnectedState();
                    }
                    if (transitioned) {
                        self->stateInterface.finishDisconnect();
                    }
                });
        auto connectedToken =
            pendingConnection->on<pendingconnectionevents::Connected>(
                [weakSelf, endpointPin, rawPendingConnection](
                    const PeerDescriptor& /*peerDescriptor*/,
                    const std::shared_ptr<Connection>& connection) {
                    const auto pin = endpointPin.lock();
                    if (!pin) {
                        return;
                    }
                    const auto self = weakSelf.lock();
                    if (!self) {
                        return;
                    }
                    bool transitioned = false;
                    {
                        std::scoped_lock lock(
                            self->stateInterface.getStateMachineMutex());
                        if (self->pendingConnection.get() !=
                            rawPendingConnection) {
                            return; // stale handler of a replaced pending
                                    // connection
                        }
                        self->detachFromPendingConnection();
                        // Flushing under the mutex keeps the buffered
                        // bytes ordered before any send() that observes
                        // the connected state (connection->send() emits
                        // nothing, so it is safe to call here).
                        if (!self->buffer.empty()) {
                            connection->send(self->buffer);
                            self->buffer.clear();
                        }
                        self->stateInterface.enterConnectedState(connection);
                        transitioned = true;
                    }
                    if (transitioned) {
                        self->stateInterface.emitConnected();
                    }
                });
        {
            std::scoped_lock lock(this->stateInterface.getStateMachineMutex());
            if (this->pendingConnection == pendingConnection) {
                this->disconnectedHandlerToken = disconnectedToken;
                this->connectedHandlerToken = connectedToken;
                SLogger::debug(
                    "ConnectingEndpointState::registerEventHandlers end");
                return;
            }
        }
        pendingConnection->off<pendingconnectionevents::Disconnected>(
            disconnectedToken);
        pendingConnection->off<pendingconnectionevents::Connected>(
            connectedToken);
        SLogger::debug(
            "ConnectingEndpointState::registerEventHandlers end (machine"
            " moved on during registration)");
    }

    [[nodiscard]] DeferredCallout close(bool graceful) override {
        SLogger::debug("ConnectingEndpointState::close");
        auto pendingConnection = this->pendingConnection;
        this->detachFromPendingConnection();
        this->buffer.clear();
        if (!pendingConnection) {
            return {};
        }
        // pendingConnection->close() emits its Disconnected event, so it
        // must run after the mutex is released.
        return [pendingConnection, graceful]() {
            pendingConnection->close(graceful);
        };
    }

    void send(const std::vector<std::byte>& data) override {
        SLogger::debug("ConnectingEndpointState::send");
        this->buffer.insert(this->buffer.end(), data.begin(), data.end());
    }

    [[nodiscard]] DeferredCallout changeToConnectingState(
        const std::shared_ptr<IPendingConnection>& pendingConnection) override {
        SLogger::debug("ConnectingEndpointState::changeToConnectingState");
        // Simultaneous-connect tiebreak: the new pending connection
        // replaces the current one.
        this->stateInterface.enterConnectingState(pendingConnection);
        return {};
    }

    [[nodiscard]] DeferredCallout changeToConnectedState(
        const std::shared_ptr<Connection>& connection) override {
        SLogger::debug("ConnectingEndpointState::changeToConnectedState");
        this->detachFromPendingConnection();
        if (!this->buffer.empty()) {
            connection->send(this->buffer);
            this->buffer.clear();
        }
        this->stateInterface.enterConnectedState(connection);
        return {};
    }

    [[nodiscard]] bool isConnected() const override {
        SLogger::debug("ConnectingEndpointState::isConnected");
        return false;
    }
};

} // namespace streamr::dht::connection::endpoint
