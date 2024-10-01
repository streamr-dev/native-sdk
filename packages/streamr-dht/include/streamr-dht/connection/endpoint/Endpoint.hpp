#ifndef STREAMR_DHT_ENDPOINTS_HPP
#define STREAMR_DHT_ENDPOINTS_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/connection/endpoint/ConnectedEndpointState.hpp"
#include "streamr-dht/connection/endpoint/ConnectingEndpointState.hpp"
#include "streamr-dht/connection/endpoint/DisconnectedEndpointState.hpp"
#include "streamr-dht/connection/endpoint/EndpointState.hpp"
#include "streamr-dht/connection/endpoint/EndpointStateInterface.hpp"
#include "streamr-dht/connection/endpoint/InitialEndpointState.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/EnableSharedFromThis.hpp"

namespace streamr::dht::connection::endpoint {

using ::dht::PeerDescriptor;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;

namespace endpointevents {

struct Connected : Event<> {};
struct Disconnected : Event<> {};
struct Data : Event<std::vector<std::byte>> {};

} // namespace endpointevents

using EndpointEvents = std::tuple<
    endpointevents::Data,
    endpointevents::Connected,
    endpointevents::Disconnected>;

class Endpoint : public EventEmitter<EndpointEvents>,
                 public EnableSharedFromThis {
private:
    std::function<void()> removeSelfFromContainer;
    EndpointState* state;
    std::recursive_mutex mutex;

    void emitData(const std::vector<std::byte>& data) {
        SLogger::debug("Endpoint::emitData start");
        auto self = sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        this->emit<endpointevents::Data>(data);
        SLogger::debug("Endpoint::emitData end");
    }

    void handleDisconnect(bool /*gracefulLeave*/) {
        SLogger::debug("Endpoint::handleDisconnect start");
        auto self = sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        this->state = this->disconnectedState.get();
        this->emit<endpointevents::Disconnected>();
        this->removeAllListeners();
        this->removeSelfFromContainer();
        SLogger::debug("Endpoint::handleDisconnect end");
    }

    void changeToConnectedState(const std::shared_ptr<Connection>& connection) {
        SLogger::debug("Endpoint::changeToConnectedState start");
        auto self = sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        this->state = this->connectedState.get();
        this->connectedState->enterState(connection);
        SLogger::debug("Endpoint::changeToConnectedState end");
    }

    void handleConnected() {
        SLogger::debug("Endpoint::handleConnected start");
        auto self = sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        this->emit<endpointevents::Connected>();
        SLogger::debug("Endpoint::handleConnected end");
    }

    friend class EndpointStateInterface;

    EndpointStateInterface stateInterface;
    std::shared_ptr<InitialEndpointState> initialState;
    std::shared_ptr<ConnectedEndpointState> connectedState;
    std::shared_ptr<ConnectingEndpointState> connectingState;
    std::shared_ptr<DisconnectedEndpointState> disconnectedState;
    PeerDescriptor peerDescriptor;

    explicit Endpoint(
        PeerDescriptor peerDescriptor,
        std::function<void()>&& removeSelfFromContainer)
        : stateInterface(*this),
          connectedState(
              ConnectedEndpointState::newInstance(this->stateInterface)),
          connectingState(
              ConnectingEndpointState::newInstance(this->stateInterface)),
          disconnectedState(
              DisconnectedEndpointState::newInstance(this->stateInterface)),
          initialState(InitialEndpointState::newInstance(this->stateInterface)),
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

    void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection) {
        SLogger::debug("Endpoint::changeToConnectingState start");
        auto self = sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        this->state = this->connectingState.get();
        this->connectingState->enterState(pendingConnection);
        SLogger::debug("Endpoint::changeToConnectingState end");
    }

    [[nodiscard]] bool isConnected() {
        SLogger::debug("Endpoint::isConnected");
        std::scoped_lock lock(this->mutex);
        return this->state->isConnected();
    }

    [[nodiscard]] PeerDescriptor getPeerDescriptor() {
        SLogger::debug("Endpoint::getPeerDescriptor");
        SLogger::debug("Endpoint::getPeerDescriptor acquiring scoped lock");
        std::scoped_lock lock(this->mutex);
        SLogger::debug("Endpoint::getPeerDescriptor got scoped lock");
        return this->peerDescriptor;
    }

    void close(bool graceful) {
        SLogger::debug("Endpoint::close start");
        auto self = sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        this->state->close(graceful);
        this->removeAllListeners();
        this->removeSelfFromContainer();
        SLogger::debug("Endpoint::close end");
    }

    void send(const std::vector<std::byte>& data) {
        SLogger::debug("Endpoint::send() start");
        SLogger::debug("Endpoint::send() acquiring scoped lock");
        auto self = sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        SLogger::debug(
            "Endpoint::send() acquired scoped lock and calling state->send()");
        this->state->send(data);
        SLogger::debug("Endpoint::send() state->send() returned");
    }

    void setConnecting(
        const std::shared_ptr<PendingConnection>& pendingConnection) {
        auto self = sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        SLogger::debug("Endpoint::setConnecting start");
        this->state->changeToConnectingState(pendingConnection);
        SLogger::debug("Endpoint::setConnecting end");
    }

    void setConnected(const std::shared_ptr<Connection>& connection) {
        SLogger::debug("Endpoint::setConnected start");
        auto self = sharedFromThis<Endpoint>();
        std::scoped_lock lock(this->mutex);
        this->state->changeToConnectedState(connection);
        SLogger::debug("Endpoint::setConnected end");
    }
};

/*
inline void ConnectedEndpointState::changeToConnectingState(
    const std::shared_ptr<PendingConnection>& pendingConnection) {
    SLogger::debug("ConnectedEndpointState::changeToConnectingState start");
    std::scoped_lock lock(this->connectedEndpointStateMutex);
    this->connection->close(true);
    this->removeEventHandlers();
    this->stateInterface.changeToConnectingState(pendingConnection);
    SLogger::debug("ConnectedEndpointState::changeToConnectingState end");
}
*/

inline EndpointStateInterface::EndpointStateInterface(Endpoint& ep)
    : endpoint(ep) {
    SLogger::debug("EndpointStateInterface constructor");
}

inline void EndpointStateInterface::changeToConnectingState(
    const std::shared_ptr<PendingConnection>& pendingConnection) {
    SLogger::debug("EndpointStateInterface::changeToConnectingState start");
    this->endpoint.changeToConnectingState(pendingConnection);
    SLogger::debug("EndpointStateInterface::changeToConnectingState end");
}

inline void EndpointStateInterface::changeToConnectedState(
    const std::shared_ptr<Connection>& connection) {
    SLogger::debug("EndpointStateInterface::changeToConnectedState start");
    this->endpoint.changeToConnectedState(connection);
    SLogger::debug("EndpointStateInterface::changeToConnectedState end");
}

inline void EndpointStateInterface::emitData(
    const std::vector<std::byte>& data) {
    SLogger::debug("EndpointStateInterface::emitData start");
    this->endpoint.emitData(data);
    SLogger::debug("EndpointStateInterface::emitData end");
}

inline void EndpointStateInterface::handleDisconnect(bool gracefulLeave) {
    SLogger::debug("EndpointStateInterface::handleDisconnect start");
    SLogger::debug(
        "EndpointStateInterface::handleDisconnect acquiring scoped lock");
    std::scoped_lock lock(this->endpoint.mutex);
    SLogger::debug("EndpointStateInterface::handleDisconnect lock acquired");
    this->endpoint.handleDisconnect(gracefulLeave);
    SLogger::debug("EndpointStateInterface::handleDisconnect end");
}

inline void EndpointStateInterface::handleConnected() {
    SLogger::debug("EndpointStateInterface::handleConnected start");

    this->endpoint.handleConnected();
    SLogger::debug("EndpointStateInterface::handleConnected end");
}

} // namespace streamr::dht::connection::endpoint

#endif