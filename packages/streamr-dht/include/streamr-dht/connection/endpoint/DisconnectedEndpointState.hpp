#ifndef STREAMR_DHT_CONNECTION_ENDPOINT_DISCONNECTEDENDPOINTSTATE_HPP
#define STREAMR_DHT_CONNECTION_ENDPOINT_DISCONNECTEDENDPOINTSTATE_HPP

#include <cstddef>
#include <memory>
#include <vector>

#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-dht/connection/endpoint/EndpointState.hpp"
#include "streamr-dht/connection/endpoint/EndpointStateInterface.hpp"
#include "streamr-dht/helpers/Errors.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::helpers::SendFailed;
using streamr::logger::SLogger;

class DisconnectedEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;

    explicit DisconnectedEndpointState(EndpointStateInterface& stateInterface)
        : stateInterface(stateInterface) {
        SLogger::debug("DisconnectedEndpointState constructor");
    }

public:
    [[nodiscard]] static std::shared_ptr<DisconnectedEndpointState> newInstance(
        EndpointStateInterface& stateInterface) {
        struct MakeSharedEnabler : public DisconnectedEndpointState {
            explicit MakeSharedEnabler(EndpointStateInterface& stateInterface)
                : DisconnectedEndpointState(stateInterface) {}
        };
        return std::make_shared<MakeSharedEnabler>(stateInterface);
    }

    ~DisconnectedEndpointState() override {
        SLogger::debug("DisconnectedEndpointState destructor");
    }

    void close(bool /*graceful*/) override {
        SLogger::debug("DisconnectedEndpointState::close start");
        SLogger::debug("DisconnectedEndpointState::close end");
    }

    void send(const std::vector<std::byte>& /* data */) override {
        SLogger::debug("DisconnectedEndpointState::send start");
        SLogger::debug("DisconnectedEndpointState::send end");
        throw SendFailed("send() called on disconnected endpoint");
    }

    void changeToConnectingState(
        const std::shared_ptr<IPendingConnection>& /*pendingConnection*/)
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

} // namespace streamr::dht::connection::endpoint

#endif // STREAMR_DHT_CONNECTION_ENDPOINT_DISCONNECTEDENDPOINTSTATE_HPP