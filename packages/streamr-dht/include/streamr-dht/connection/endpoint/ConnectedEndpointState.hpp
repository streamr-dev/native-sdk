#ifndef STREAMR_DHT_CONNECTION_ENDPOINT_CONNECTEDENDPOINTSTATE_HPP
#define STREAMR_DHT_CONNECTION_ENDPOINT_CONNECTEDENDPOINTSTATE_HPP

#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/endpoint/EndpointState.hpp"
#include "streamr-dht/connection/endpoint/EndpointStateInterface.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;

class ConnectedEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;
    std::shared_ptr<Connection> connection;
    HandlerToken dataHandlerToken;
    HandlerToken disconnectHandlerToken;
    std::recursive_mutex connectedEndpointStateMutex;

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
        if (this->connection) {
            SLogger::debug("ConnectedEndpointState destructor " + this->connection->getConnectionTypeString());
        }
    }

    void enterState(const std::shared_ptr<Connection>& connection) {
        SLogger::debug("ConnectedEndpointState::enterState start");
        auto self = sharedFromThis<ConnectedEndpointState>();
        std::scoped_lock lock(this->connectedEndpointStateMutex);
        this->connection = connection;
        this->dataHandlerToken = this->connection->on<connectionevents::Data>(
            [this](const std::vector<std::byte>& data) {
                SLogger::debug("ConnectedEndpointState::enterState data");
                std::scoped_lock lock(this->connectedEndpointStateMutex);
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
                    std::scoped_lock lock(this->connectedEndpointStateMutex);
                    auto self = sharedFromThis<ConnectedEndpointState>();
                    self->stateInterface.handleDisconnect(gracefulLeave);
                    SLogger::debug(
                        "ConnectedEndpointState::enterState disconnected end");
                });
        SLogger::debug("ConnectedEndpointState::enterState end");
    }

    void removeEventHandlers() {
        SLogger::debug("ConnectedEndpointState::removeEventHandlers start");
        auto self = sharedFromThis<ConnectedEndpointState>();
        std::scoped_lock lock(this->connectedEndpointStateMutex);
        if (this->connection) {
            this->connection->off<connectionevents::Data>(
                this->dataHandlerToken);
            this->connection->off<connectionevents::Disconnected>(
                this->disconnectHandlerToken);
        }
        SLogger::debug("ConnectedEndpointState::removeEventHandlers end");
    }

    void close(bool graceful) override {
        SLogger::debug("ConnectedEndpointState::close start " + this->connection->getConnectionTypeString());
        auto self = sharedFromThis<ConnectedEndpointState>();
        std::scoped_lock lock(this->connectedEndpointStateMutex);
        this->removeEventHandlers();
        this->connection->close(graceful);
        SLogger::debug("ConnectedEndpointState::close end " + this->connection->getConnectionTypeString());
    }

    void send(const std::vector<std::byte>& data) override {
        SLogger::debug("ConnectedEndpointState::send start");
        auto self = sharedFromThis<ConnectedEndpointState>();
        SLogger::debug("ConnectedEndpointState::send acquiring scoped lock");
        std::scoped_lock lock(this->connectedEndpointStateMutex);
        SLogger::debug("ConnectedEndpointState::send lock acquired");
        this->connection->send(data);
        SLogger::debug("ConnectedEndpointState::send end");
    }

    void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection) override {
        SLogger::debug("ConnectedEndpointState::changeToConnectingState start");
        auto self = sharedFromThis<ConnectedEndpointState>();
        std::scoped_lock lock(this->connectedEndpointStateMutex);
        this->connection->close(true);
        this->removeEventHandlers();
        this->stateInterface.changeToConnectingState(pendingConnection);
        SLogger::debug("ConnectedEndpointState::changeToConnectingState end");
    }

    void changeToConnectedState(
        const std::shared_ptr<Connection>& connection) override {
        SLogger::debug("ConnectedEndpointState::changeToConnectedState start");
        auto self = sharedFromThis<ConnectedEndpointState>();
        std::scoped_lock lock(this->connectedEndpointStateMutex);
        this->removeEventHandlers();
        this->enterState(connection);
        SLogger::debug("ConnectedEndpointState::changeToConnectedState end");
    }

    [[nodiscard]] bool isConnected() const override {
        SLogger::debug("ConnectedEndpointState::isConnected");
        return true;
    }
};

} // namespace streamr::dht::connection::endpoint

#endif // STREAMR_DHT_CONNECTION_ENDPOINT_CONNECTEDENDPOINTSTATE_HPP
