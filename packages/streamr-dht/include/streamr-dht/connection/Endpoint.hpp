#ifndef STREAMR_DHT_ENDPOINTS_HPP
#define STREAMR_DHT_ENDPOINTS_HPP

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection {

using ::dht::PeerDescriptor;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;

class Endpoint;
class EndpointState;

class EndpointStateInterface {
private:
    Endpoint& endpoint;

public:
    explicit EndpointStateInterface(Endpoint& ep);
    void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection);
    void changeToConnectedState(const std::shared_ptr<Connection>& connection);
    void emitData(const std::vector<std::byte>& data);
    void handleDisconnect(bool gracefulLeave);
    void handleConnected();
};

class EndpointState {
protected:
    EndpointState() {
        SLogger::debug("EndpointState constructor");
    }

public:
    virtual ~EndpointState() {
        SLogger::debug("EndpointState destructor");
    }

    virtual void close(bool graceful){
        SLogger::debug("EndpointState::close start");
        SLogger::debug("EndpointState::close end");
    };
    virtual void send(const std::vector<std::byte>& data){
        SLogger::debug("EndpointState::send start");
        SLogger::debug("EndpointState::send end");
    };

    virtual void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection) = 0;

    virtual void changeToConnectedState(
        const std::shared_ptr<Connection>& connection) = 0;

    [[nodiscard]] virtual bool isConnected() const = 0;
};

class ConnectedEndpointState : public EndpointState, public std::enable_shared_from_this<ConnectedEndpointState> {
private:
    EndpointStateInterface& stateInterface;
    std::shared_ptr<Connection> connection;
    HandlerToken dataHandlerToken;
    HandlerToken disconnectHandlerToken;

public:
    explicit ConnectedEndpointState(EndpointStateInterface& stateInterface)
        : stateInterface(stateInterface) {
        SLogger::debug("ConnectedEndpointState constructor");
    }

    ~ConnectedEndpointState() override {
        SLogger::debug("ConnectedEndpointState destructor");
    }

    void enterState(const std::shared_ptr<Connection>& connection) {
        SLogger::debug("ConnectedEndpointState::enterState start");
        this->connection = connection;
        this->dataHandlerToken = this->connection->on<connectionevents::Data>(
            [this](const std::vector<std::byte>& data) {
                SLogger::debug("ConnectedEndpointState::enterState data");
                auto self = shared_from_this();
                self->stateInterface.emitData(data);
                SLogger::debug("ConnectedEndpointState::enterState data end");
            });
        this->disconnectHandlerToken =
            this->connection->on<connectionevents::Disconnected>(
                [this](
                    bool gracefulLeave,
                    uint64_t /*code*/,
                    const std::string& /*reason*/) {
                    SLogger::debug("ConnectedEndpointState::enterState disconnected");
                    auto self = shared_from_this();
                    self->stateInterface.handleDisconnect(gracefulLeave);
                    SLogger::debug("ConnectedEndpointState::enterState disconnected end");
                });
        SLogger::debug("ConnectedEndpointState::enterState end");
    }

    void removeEventHandlers() {
        SLogger::debug("ConnectedEndpointState::removeEventHandlers start");
        if (this->connection) {
            this->connection->off<connectionevents::Data>(
                this->dataHandlerToken);
            this->connection->off<connectionevents::Disconnected>(
                this->disconnectHandlerToken);
        }
        SLogger::debug("ConnectedEndpointState::removeEventHandlers end");
    }

    void close(bool graceful) override {
        SLogger::debug("ConnectedEndpointState::close start");
        this->removeEventHandlers();
        this->connection->close(graceful);
        SLogger::debug("ConnectedEndpointState::close end");
    }

    void send(const std::vector<std::byte>& data) override {
        SLogger::debug("ConnectedEndpointState::send start");
        this->connection->send(data);
        SLogger::debug("ConnectedEndpointState::send end");
    }

    void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection) override;

    void changeToConnectedState(
        const std::shared_ptr<Connection>& connection) override {
        SLogger::debug("ConnectedEndpointState::changeToConnectedState start");
        this->removeEventHandlers();
        this->enterState(connection);
        SLogger::debug("ConnectedEndpointState::changeToConnectedState end");
    }

    [[nodiscard]] bool isConnected() const override {
        SLogger::debug("ConnectedEndpointState::isConnected");
        return true;
    }
};

class ConnectingEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;
    std::vector<std::byte> buffer;
    std::shared_ptr<PendingConnection> pendingConnection;

    HandlerToken connectedHandlerToken;
    HandlerToken disconnectedHandlerToken;

public:
    explicit ConnectingEndpointState(EndpointStateInterface& stateInterface)
        : stateInterface(stateInterface) {
        SLogger::debug("ConnectingEndpointState constructor");
    }

    ~ConnectingEndpointState() override {
        SLogger::debug("ConnectingEndpointState destructor");
    }

    void enterState(
        const std::shared_ptr<PendingConnection>& pendingConnection) {
        SLogger::debug("ConnectingEndpointState::enterState start");
        this->pendingConnection = pendingConnection;
        this->pendingConnection->on<
            pendingconnectionevents::Disconnected>([this](bool gracefulLeave) {
            this->pendingConnection->off<pendingconnectionevents::Disconnected>(
                this->disconnectedHandlerToken);
            this->pendingConnection->off<pendingconnectionevents::Connected>(
                this->connectedHandlerToken);

            this->stateInterface.handleDisconnect(gracefulLeave);
        });
        this->pendingConnection->on<pendingconnectionevents::Connected>(
            [this](
                const PeerDescriptor& /*peerDescriptor*/,
                const std::shared_ptr<Connection>& connection) {
                this->changeToConnectedState(connection);
                this->stateInterface.handleConnected();
            });
        SLogger::debug("ConnectingEndpointState::enterState end");
    }

    void close(bool graceful) override {
        SLogger::debug("ConnectingEndpointState::close start");
        this->removeEventHandlers();
        this->pendingConnection->close(graceful);
        SLogger::debug("ConnectingEndpointState::close end");
    }

    void send(const std::vector<std::byte>& data) override {
        SLogger::debug("ConnectingEndpointState::send start");
        this->buffer.insert(this->buffer.end(), data.begin(), data.end());
        SLogger::debug("ConnectingEndpointState::send end");
    }

    void removeEventHandlers() {
        SLogger::debug("ConnectingEndpointState::removeEventHandlers start");
        if (this->pendingConnection) {
            this->pendingConnection->off<pendingconnectionevents::Connected>(
                this->connectedHandlerToken);
            this->pendingConnection->off<pendingconnectionevents::Disconnected>(
                this->disconnectedHandlerToken);
        }
        SLogger::debug("ConnectingEndpointState::removeEventHandlers end");
    }

    void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection) override {
        SLogger::debug("ConnectingEndpointState::changeToConnectingState start");
        this->removeEventHandlers();
        this->enterState(pendingConnection);
        SLogger::debug("ConnectingEndpointState::changeToConnectingState end");
    }

    void changeToConnectedState(
        const std::shared_ptr<Connection>& connection) override {
        SLogger::debug("ConnectingEndpointState::changeToConnectedState start");
        this->removeEventHandlers();

        if (!this->buffer.empty()) {
            connection->send(this->buffer);
        }
        this->stateInterface.changeToConnectedState(connection);
        SLogger::debug("ConnectingEndpointState::changeToConnectedState end");
    }
    [[nodiscard]] bool isConnected() const override {
        SLogger::debug("ConnectingEndpointState::isConnected");
        return false;
    }
};

namespace endpointevents {
struct Connected : Event<> {};
struct Disconnected : Event<> {};
struct Data : Event<std::vector<std::byte>> {};
} // namespace endpointevents

using EndpointEvents = std::tuple<
    endpointevents::Data,
    endpointevents::Connected,
    endpointevents::Disconnected>;

class Endpoint : public EventEmitter<EndpointEvents>, public std::enable_shared_from_this<Endpoint> {
private:
    std::function<void()> removeSelfFromContainer;
    EndpointState* state;

    void emitData(const std::vector<std::byte>& data) {
        SLogger::debug("Endpoint::emitData start");
        auto self = shared_from_this();
        this->emit<endpointevents::Data>(data);
        SLogger::debug("Endpoint::emitData end");
    }

    void handleDisconnect(bool gracefulLeave) {
        SLogger::debug("Endpoint::handleDisconnect start");
        this->emit<endpointevents::Disconnected>();
        this->removeAllListeners();
        auto self = shared_from_this();
        this->removeSelfFromContainer();
        SLogger::debug("Endpoint::handleDisconnect end");
    }

    void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection) {
        SLogger::debug("Endpoint::changeToConnectingState start");
        this->state = this->connectingState.get();
        this->connectingState->enterState(pendingConnection);
        SLogger::debug("Endpoint::changeToConnectingState end");
    }

    void changeToConnectedState(const std::shared_ptr<Connection>& connection) {
        SLogger::debug("Endpoint::changeToConnectedState start");
        this->state = this->connectedState.get();
        this->connectedState->enterState(connection);
        SLogger::debug("Endpoint::changeToConnectedState end");
    }

    void handleConnected() {
        SLogger::debug("Endpoint::handleConnected start");
        auto self = shared_from_this();
        this->emit<endpointevents::Connected>();
        SLogger::debug("Endpoint::handleConnected end");
    }

    friend class EndpointStateInterface;

    EndpointStateInterface stateInterface;
    std::shared_ptr<ConnectedEndpointState> connectedState;
    std::shared_ptr<ConnectingEndpointState> connectingState;
    PeerDescriptor peerDescriptor;

public:
    explicit Endpoint(
        const std::shared_ptr<PendingConnection>& pendingConnection,
        const std::function<void()>& removeSelfFromContainer)
        : stateInterface(*this),
          connectedState(std::make_shared<ConnectedEndpointState>(this->stateInterface)),
          connectingState(std::make_shared<ConnectingEndpointState>(this->stateInterface)),
          removeSelfFromContainer(removeSelfFromContainer) {
        SLogger::debug("Endpoint constructor start");
        this->state = this->connectingState.get();
        this->changeToConnectingState(pendingConnection);
        this->peerDescriptor = pendingConnection->getPeerDescriptor();
        SLogger::debug("Endpoint constructor end");
    }

    virtual ~Endpoint() {
        SLogger::debug("Endpoint destructor");
    }

    [[nodiscard]] bool isConnected() const {
        SLogger::debug("Endpoint::isConnected");
        return this->state->isConnected();
    }

    [[nodiscard]] PeerDescriptor getPeerDescriptor() const {
        SLogger::debug("Endpoint::getPeerDescriptor");
        return this->peerDescriptor;
    }

    void close(bool graceful) {
        SLogger::debug("Endpoint::close start");
        this->state->close(graceful);
        this->removeAllListeners();
        auto self = shared_from_this();
        this->removeSelfFromContainer();
        SLogger::debug("Endpoint::close end");
    }

    void send(const std::vector<std::byte>& data) {
        SLogger::debug("Endpoint::send start");
        this->state->send(data);
        SLogger::debug("Endpoint::send end");
    }

    void setConnecting(
        const std::shared_ptr<PendingConnection>& pendingConnection) {
        SLogger::debug("Endpoint::setConnecting start");
        this->state->changeToConnectingState(pendingConnection);
        SLogger::debug("Endpoint::setConnecting end");
    }

    void setConnected(const std::shared_ptr<Connection>& connection) {
        SLogger::debug("Endpoint::setConnected start");
        this->state->changeToConnectedState(connection);
        SLogger::debug("Endpoint::setConnected end");
    }
};

inline void ConnectedEndpointState::changeToConnectingState(
    const std::shared_ptr<PendingConnection>& pendingConnection) {
    SLogger::debug("ConnectedEndpointState::changeToConnectingState start");
    this->connection->close(true);
    this->removeEventHandlers();
    this->stateInterface.changeToConnectingState(pendingConnection);
    SLogger::debug("ConnectedEndpointState::changeToConnectingState end");
}

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
    this->endpoint.handleDisconnect(gracefulLeave);
    SLogger::debug("EndpointStateInterface::handleDisconnect end");
}

inline void EndpointStateInterface::handleConnected() {
    SLogger::debug("EndpointStateInterface::handleConnected start");
    this->endpoint.handleConnected();
    SLogger::debug("EndpointStateInterface::handleConnected end");
}

} // namespace streamr::dht::connection

#endif