#ifndef STREAMR_DHT_CONNECTION_ENDPOINT_CONNECTINGENDPOINTSTATE_HPP
#define STREAMR_DHT_CONNECTION_ENDPOINT_CONNECTINGENDPOINTSTATE_HPP

#include <memory>
#include <mutex>
#include <vector>
#include "streamr-dht/connection/endpoint/EndpointState.hpp"
#include "streamr-dht/connection/endpoint/EndpointStateInterface.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"

namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::PendingConnection;
using streamr::eventemitter::HandlerToken;

class ConnectingEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;
    std::vector<std::byte> buffer;
    std::shared_ptr<PendingConnection> pendingConnection;
    HandlerToken connectedHandlerToken;
    HandlerToken disconnectedHandlerToken;
    std::recursive_mutex connectingEndpointStateMutex;

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
        auto self = sharedFromThis<ConnectingEndpointState>();
        std::scoped_lock lock(this->connectingEndpointStateMutex);
        this->pendingConnection = pendingConnection;
        this->pendingConnection->on<pendingconnectionevents::Disconnected>(
            [this](bool gracefulLeave) {
                std::shared_ptr<ConnectingEndpointState> self;
                {
                    std::scoped_lock lock(this->connectingEndpointStateMutex);
                    self = sharedFromThis<ConnectingEndpointState>();
                    self->pendingConnection
                        ->off<pendingconnectionevents::Disconnected>(
                            this->disconnectedHandlerToken);
                    self->pendingConnection
                        ->off<pendingconnectionevents::Connected>(
                            this->connectedHandlerToken);
                }
                self->stateInterface.handleDisconnect(gracefulLeave);
            });
        this->pendingConnection->on<pendingconnectionevents::Connected>(
            [this](
                const PeerDescriptor& /*peerDescriptor*/,
                const std::shared_ptr<Connection>& connection) {
                std::shared_ptr<ConnectingEndpointState> self;
                {
                    std::scoped_lock lock(this->connectingEndpointStateMutex);
                    self = sharedFromThis<ConnectingEndpointState>();
                }
                self->changeToConnectedState(connection);
                self->stateInterface.handleConnected();
            });
        SLogger::debug("ConnectingEndpointState::enterState end");
    }

    void close(bool graceful) override {
        SLogger::debug("ConnectingEndpointState::close start");
        auto self = sharedFromThis<ConnectingEndpointState>();
        std::scoped_lock lock(this->connectingEndpointStateMutex);
        this->removeEventHandlers();
        this->pendingConnection->close(graceful);
        SLogger::debug("ConnectingEndpointState::close end");
    }

    void send(const std::vector<std::byte>& data) override {
        SLogger::debug("ConnectingEndpointState::send start");
        auto self = sharedFromThis<ConnectingEndpointState>();
        SLogger::debug("ConnectingEndpointState::send acquiring scoped lock");
        std::scoped_lock lock(this->connectingEndpointStateMutex);
        SLogger::debug("ConnectingEndpointState::send lock acquired");
        this->buffer.insert(this->buffer.end(), data.begin(), data.end());
        SLogger::debug("ConnectingEndpointState::send end");
    }

    void removeEventHandlers() {
        SLogger::debug("ConnectingEndpointState::removeEventHandlers start");
        auto self = sharedFromThis<ConnectingEndpointState>();
        std::scoped_lock lock(this->connectingEndpointStateMutex);
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
        auto self = sharedFromThis<ConnectingEndpointState>();
        std::scoped_lock lock(this->connectingEndpointStateMutex);
        this->removeEventHandlers();
        this->enterState(pendingConnection);
        SLogger::debug("ConnectingEndpointState::changeToConnectingState end");
    }

    void changeToConnectedState(
        const std::shared_ptr<Connection>& connection) override {
        SLogger::debug("ConnectingEndpointState::changeToConnectedState start");
        auto self = sharedFromThis<ConnectingEndpointState>();
        std::scoped_lock lock(this->connectingEndpointStateMutex);
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

} // namespace streamr::dht::connection::endpoint
#endif // STREAMR_DHT_CONNECTION_ENDPOINT_CONNECTINGENDPOINTSTATE_HPP