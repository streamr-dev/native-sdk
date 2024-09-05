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

namespace streamr::dht::connection {

using ::dht::PeerDescriptor;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::HandlerToken;

class Endpoint;
class EndpointState;

// Forward declaration of EndpointStateInterface
// that Enpoint states can use to access the Endpoint

class EndpointStateInterface {
private:
    Endpoint& endpoint;

public:
    explicit EndpointStateInterface(Endpoint& ep);
    void changeState(const std::shared_ptr<EndpointState>& newState);
    void emitData(const std::vector<std::byte>& data);
    void handleDisconnect(bool gracefulLeave);
    void handleConnected();
};

class EndpointState {
protected:
    EndpointState() = default;

public:
    virtual ~EndpointState() = default;

    virtual void close(bool graceful){};
    virtual void send(const std::vector<std::byte>& data){};

    // Following methods are defined inline in the end of this file
    virtual void changeToConnectingState(
        EndpointStateInterface& stateInterface,
        const std::shared_ptr<PendingConnection>& pendingConnection) = 0;
    virtual void changeToConnectedState(
        EndpointStateInterface& stateInterface,
        const std::shared_ptr<Connection>& connection) = 0;
    [[nodiscard]] virtual bool isConnected() const = 0;
};

class ConnectedEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;
    std::shared_ptr<Connection> connection;
    HandlerToken dataHandlerToken;
    HandlerToken disconnectHandlerToken;

public:
    explicit ConnectedEndpointState(
        EndpointStateInterface& stateInterface,
        const std::shared_ptr<Connection>& connection)
        : stateInterface(stateInterface), connection(connection) {
        this->dataHandlerToken = this->connection->on<connectionevents::Data>(
            [this](const std::vector<std::byte>& data) {
                this->stateInterface.emitData(data);
            });
        this->disconnectHandlerToken =
            this->connection->on<connectionevents::Disconnected>(
                [this](
                    bool gracefulLeave,
                    uint64_t /*code*/,
                    const std::string& /*reason*/) {
                    this->stateInterface.handleDisconnect(gracefulLeave);
                });
    }

    ~ConnectedEndpointState() override = default;

    void close(bool graceful) override {
        this->connection->off<connectionevents::Data>(this->dataHandlerToken);
        this->connection->off<connectionevents::Disconnected>(
            this->disconnectHandlerToken);
        this->connection->close(graceful);
    }

    void send(const std::vector<std::byte>& data) override {
        this->connection->send(data);
    }

    // Following method is defined inline in the end of this file
    void changeToConnectingState(
        EndpointStateInterface& stateInterface,
        const std::shared_ptr<PendingConnection>& pendingConnection) override;

    void changeToConnectedState(
        EndpointStateInterface& /*stateInterface*/,
        const std::shared_ptr<Connection>& connection) override {
        // we are alrady in connected state, only change the connection

        this->connection->off<connectionevents::Data>(this->dataHandlerToken);
        this->connection = connection;
        this->dataHandlerToken = this->connection->on<connectionevents::Data>(
            [this](const std::vector<std::byte>& data) {
                this->stateInterface.emitData(data);
            });
    }
    [[nodiscard]] bool isConnected() const override { return true; }
};

class ConnectingEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;
    std::vector<std::byte> buffer;
    std::shared_ptr<PendingConnection> pendingConnection;

    HandlerToken connectedHandlerToken;
    HandlerToken disconnectedHandlerToken;

public:
    explicit ConnectingEndpointState(
        EndpointStateInterface& stateInterface,
        const std::shared_ptr<PendingConnection>& pendingConnection)
        : stateInterface(stateInterface), pendingConnection(pendingConnection) {
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
                this->changeToConnectedState(this->stateInterface, connection);
                this->stateInterface.handleConnected();
            });
    }
    ~ConnectingEndpointState() override = default;

    void close(bool graceful) override {
        this->pendingConnection->close(graceful);
    }

    void send(const std::vector<std::byte>& data) override {
        this->buffer.insert(this->buffer.end(), data.begin(), data.end());
    }

    void changeToConnectingState(
        EndpointStateInterface& /*stateInterface*/,
        const std::shared_ptr<PendingConnection>& pendingConnection) override {
        // we are already connecting, only change the pending connection
        this->pendingConnection = pendingConnection;
    }

    void changeToConnectedState(
        EndpointStateInterface& stateInterface,
        const std::shared_ptr<Connection>& connection) override {
        this->pendingConnection->off<pendingconnectionevents::Connected>(
            this->connectedHandlerToken);
        this->pendingConnection->off<pendingconnectionevents::Disconnected>(
            this->disconnectedHandlerToken);
        if (!this->buffer.empty()) {
            connection->send(this->buffer);
        }
        stateInterface.changeState(
            std::make_shared<ConnectedEndpointState>(stateInterface, connection));
    }
    [[nodiscard]] bool isConnected() const override { return false; }
};

namespace endpointevents {
struct Connected : Event<> {};
struct Disconnected : Event<> {};
struct Data : Event<std::vector<std::byte>> {};
} // namespace endpointevents

using EndpointEvents =
    std::tuple<endpointevents::Data, endpointevents::Connected, endpointevents::Disconnected>;

class Endpoint : public EventEmitter<EndpointEvents> {
public:
    /*
    using EventEmitter<EndpointEvents>::once;
    using EventEmitter<EndpointEvents>::on;
    using EventEmitter<EndpointEvents>::off;
    using EventEmitter<EndpointEvents>::emit;
    */

private:
    std::function<void()> removeSelfFromContainer;
    std::shared_ptr<EndpointState> state;

    void changeState(const std::shared_ptr<EndpointState>& newState) {
        this->state = newState;
    }
    void emitData(const std::vector<std::byte>& data) {
        this->emit<endpointevents::Data>(data);
    }

    void handleDisconnect(bool /*gracefulLeave*/) {
        this->emit<endpointevents::Disconnected>();
        this->removeAllListeners();
        this->removeSelfFromContainer();
    }

    void handleConnected() {
        this->emit<endpointevents::Connected>();
    }

    friend class EndpointStateInterface;

    EndpointStateInterface stateInterface;
    PeerDescriptor peerDescriptor;

public:
    explicit Endpoint(
        const std::shared_ptr<PendingConnection>& pendingConnection,
        const std::function<void()>& removeSelfFromContainer)
        : stateInterface(*this),
          state(std::make_shared<ConnectingEndpointState>(
              this->stateInterface, pendingConnection)),
          removeSelfFromContainer(removeSelfFromContainer) {
        this->peerDescriptor = pendingConnection->getPeerDescriptor();
    }

    virtual ~Endpoint() = default;

    [[nodiscard]] bool isConnected() const {
        return this->state->isConnected();
    }

    [[nodiscard]] PeerDescriptor getPeerDescriptor() const {
        return this->peerDescriptor;
    }

    void close(bool graceful) {
        this->state->close(graceful);
        this->removeAllListeners();
        this->removeSelfFromContainer();
    }

    void send(const std::vector<std::byte>& data) { this->state->send(data); }

    void setConnecting(
        const std::shared_ptr<PendingConnection>& pendingConnection) {
        this->state->changeToConnectingState(
            this->stateInterface, pendingConnection);
    }

    void setConnected(const std::shared_ptr<Connection>& connection) {
        this->state->changeToConnectedState(this->stateInterface, connection);
    }
};

// Inline implementatios are needed because of the circular dependencies between
// Different endpoint states.

/*
inline void EndpointState::changeToConnectingState(
    EndpointStateInterface& stateInterface,
    const std::shared_ptr<PendingConnection>& pendingConnection) {
    stateInterface.changeState(
        std::make_shared<ConnectingEndpointState>(stateInterface,
pendingConnection));
}

inline void EndpointState::changeToConnectedState(
    EndpointStateInterface& stateInterface,
    const std::shared_ptr<Connection>& connection) {
    stateInterface.changeState(
        std::make_shared<ConnectedEndpointState>(stateInterface, connection));
}
*/

inline void ConnectedEndpointState::changeToConnectingState(
    EndpointStateInterface& stateInterface,
    const std::shared_ptr<PendingConnection>& pendingConnection) {
    this->connection->close(true);
    stateInterface.changeState(std::make_shared<ConnectingEndpointState>(
        stateInterface, pendingConnection));
}

inline EndpointStateInterface::EndpointStateInterface(Endpoint& ep)
    : endpoint(ep) {}

inline void EndpointStateInterface::changeState(
    const std::shared_ptr<EndpointState>& newState) {
    this->endpoint.changeState(newState);
}

inline void EndpointStateInterface::emitData(
    const std::vector<std::byte>& data) {
    this->endpoint.emitData(data);
}

inline void EndpointStateInterface::handleDisconnect(bool gracefulLeave) {
    this->endpoint.handleDisconnect(gracefulLeave);
}

inline void EndpointStateInterface::handleConnected() {
    this->endpoint.handleConnected();
}

} // namespace streamr::dht::connection

#endif