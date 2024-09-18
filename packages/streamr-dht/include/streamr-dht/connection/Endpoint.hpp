#ifndef STREAMR_DHT_ENDPOINTS_HPP
#define STREAMR_DHT_ENDPOINTS_HPP

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/helpers/Errors.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/EnableSharedFromThis.hpp"
namespace streamr::dht::connection {

using ::dht::PeerDescriptor;
using streamr::dht::helpers::SendFailed;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;

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

class EndpointState : public EnableSharedFromThis {
protected:
    EndpointState() { SLogger::debug("EndpointState constructor"); }

public:
    ~EndpointState() override { SLogger::debug("EndpointState destructor"); }

    virtual void close(bool /*graceful*/) {
        SLogger::debug("EndpointState::close start");
        SLogger::debug("EndpointState::close end");
    };
    virtual void send(const std::vector<std::byte>& /* data */) {
        SLogger::debug("EndpointState::send start");
        SLogger::debug("EndpointState::send end");
    };

    virtual void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection) = 0;

    virtual void changeToConnectedState(
        const std::shared_ptr<Connection>& connection) = 0;

    [[nodiscard]] virtual bool isConnected() const = 0;
};

class DisconnectedEndpointState : public EndpointState {
public:
    ~DisconnectedEndpointState() override {
        SLogger::debug("DisconnectedEndpointState destructor");
    }

    void close(bool /*graceful*/) override {
        SLogger::debug("DisconnectedEndpointState::close start");
        SLogger::debug("DisconnectedEndpointState::close end");
    };
    void send(const std::vector<std::byte>& /* data */) override {
        SLogger::debug("DisconnectedEndpointState::send start");
        SLogger::debug("DisconnectedEndpointState::send end");
        throw SendFailed("send() called on disconnected endpoint");
    };

    void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& /*pendingConnection*/)
        override {
        SLogger::debug(
            "DisconnectedEndpointState::changeToConnectingState start");
        SLogger::debug(
            "DisconnectedEndpointState::changeToConnectingState end");
    }

    void changeToConnectedState(
        const std::shared_ptr<Connection>& /*connection*/) override {
        SLogger::debug(
            "DisconnectedEndpointState::changeToConnectedState start");
        SLogger::debug("DisconnectedEndpointState::changeToConnectedState end");
    }

    [[nodiscard]] bool isConnected() const override {
        SLogger::debug("DisconnectedEndpointState::isConnected");
        return false;
    }
};

class ConnectedEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;
    std::shared_ptr<Connection> connection;
    HandlerToken dataHandlerToken;
    HandlerToken disconnectHandlerToken;

    explicit ConnectedEndpointState(EndpointStateInterface& stateInterface)
        : stateInterface(stateInterface) {
        SLogger::debug("ConnectedEndpointState constructor");
    }

public:
    [[nodiscard]] static std::shared_ptr<ConnectedEndpointState> newInstance(
        EndpointStateInterface& stateInterface) {
        struct MakeSharedEnabler : public ConnectedEndpointState {
            explicit MakeSharedEnabler(EndpointStateInterface& stateInterface)
                : ConnectedEndpointState(stateInterface) {}
        };
        return std::make_shared<MakeSharedEnabler>(stateInterface);
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
                auto self = sharedFromThis<ConnectedEndpointState>();
                self->stateInterface.emitData(data);
                SLogger::debug("ConnectedEndpointState::enterState data end");
            });
        this->disconnectHandlerToken =
            this->connection->on<connectionevents::Disconnected>(
                [this](
                    bool gracefulLeave,
                    uint64_t /*code*/,
                    const std::string& /*reason*/) {
                    SLogger::debug(
                        "ConnectedEndpointState::enterState disconnected");
                    auto self = sharedFromThis<ConnectedEndpointState>();
                    self->stateInterface.handleDisconnect(gracefulLeave);
                    SLogger::debug(
                        "ConnectedEndpointState::enterState disconnected end");
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

    explicit ConnectingEndpointState(EndpointStateInterface& stateInterface)
        : stateInterface(stateInterface) {
        SLogger::debug("ConnectingEndpointState constructor");
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

    void enterState(
        const std::shared_ptr<PendingConnection>& pendingConnection) {
        this->pendingConnection = pendingConnection;
        this->pendingConnection->on<
            pendingconnectionevents::Disconnected>([this](bool gracefulLeave) {
            auto self = sharedFromThis<ConnectingEndpointState>();
            self->pendingConnection->off<pendingconnectionevents::Disconnected>(
                this->disconnectedHandlerToken);
            self->pendingConnection->off<pendingconnectionevents::Connected>(
                this->connectedHandlerToken);
            self->stateInterface.handleDisconnect(gracefulLeave);
        });
        this->pendingConnection->on<pendingconnectionevents::Connected>(
            [this](
                const PeerDescriptor& /*peerDescriptor*/,
                const std::shared_ptr<Connection>& connection) {
                auto self = sharedFromThis<ConnectingEndpointState>();
                self->changeToConnectedState(connection);
                self->stateInterface.handleConnected();
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
        SLogger::debug(
            "ConnectingEndpointState::changeToConnectingState start");
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

class Endpoint : public EventEmitter<EndpointEvents>,
                 public EnableSharedFromThis {
private:
    std::function<void()> removeSelfFromContainer;
    EndpointState* state;
    std::recursive_mutex mutex;

    void emitData(const std::vector<std::byte>& data) {
        SLogger::debug("Endpoint::emitData start");
        auto self = sharedFromThis<Endpoint>();
        this->emit<endpointevents::Data>(data);
        SLogger::debug("Endpoint::emitData end");
    }

    void handleDisconnect(bool /*gracefulLeave*/) {
        SLogger::debug("Endpoint::handleDisconnect start");
        std::scoped_lock lock(this->mutex);
        this->state = this->disconnectedState.get();

        this->emit<endpointevents::Disconnected>();
        this->removeAllListeners();
        auto self = sharedFromThis<Endpoint>();
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
        auto self = sharedFromThis<Endpoint>();
        this->emit<endpointevents::Connected>();
        SLogger::debug("Endpoint::handleConnected end");
    }

    friend class EndpointStateInterface;

    EndpointStateInterface stateInterface;
    std::shared_ptr<ConnectedEndpointState> connectedState;
    std::shared_ptr<ConnectingEndpointState> connectingState;
    std::shared_ptr<DisconnectedEndpointState> disconnectedState;
    PeerDescriptor peerDescriptor;

    Endpoint(
        std::shared_ptr<PendingConnection>&& pendingConnection,
        std::function<void()>&& removeSelfFromContainer)
        : stateInterface(*this),
          connectedState(
              ConnectedEndpointState::newInstance(this->stateInterface)),
          connectingState(
              ConnectingEndpointState::newInstance(this->stateInterface)),
          removeSelfFromContainer(removeSelfFromContainer) {
        SLogger::debug("Endpoint constructor start");
        this->state = this->connectingState.get();
        this->changeToConnectingState(pendingConnection);
        this->peerDescriptor = pendingConnection->getPeerDescriptor();
        SLogger::debug("Endpoint constructor end");
    }

public:
    [[nodiscard]] static std::shared_ptr<Endpoint> newInstance(
        std::shared_ptr<PendingConnection>&& pendingConnection,
        std::function<void()>&& removeSelfFromContainer) {
        struct MakeSharedEnabler : public Endpoint {
            explicit MakeSharedEnabler(
                std::shared_ptr<PendingConnection>&& pendingConnection,
                std::function<void()>&& removeSelfFromContainer)
                : Endpoint(
                      std::move(pendingConnection),
                      std::move(removeSelfFromContainer)) {}
        };
        return std::make_shared<MakeSharedEnabler>(
            std::move(pendingConnection), std::move(removeSelfFromContainer));
    }

    ~Endpoint() override { SLogger::debug("Endpoint destructor"); }

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
        auto self = sharedFromThis<Endpoint>();
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