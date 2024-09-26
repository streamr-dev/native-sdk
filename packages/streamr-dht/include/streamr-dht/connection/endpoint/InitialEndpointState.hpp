#ifndef STREAMR_DHT_CONNECTION_ENDPOINT_INITIALENDPOINTSTATE_HPP
#define STREAMR_DHT_CONNECTION_ENDPOINT_INITIALENDPOINTSTATE_HPP

#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/connection/endpoint/EndpointState.hpp"
#include "streamr-dht/connection/endpoint/EndpointStateInterface.hpp"
#include "streamr-dht/helpers/Errors.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::PendingConnection;
using streamr::dht::helpers::SendFailed;
using streamr::logger::SLogger;

class InitialEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;

    explicit InitialEndpointState(EndpointStateInterface& stateInterface)
        : stateInterface(stateInterface) {
        SLogger::debug("InitialEndpointState constructor");
    }

public:
    [[nodiscard]] static std::shared_ptr<InitialEndpointState> newInstance(
        EndpointStateInterface& stateInterface) {
        struct MakeSharedEnabler : public InitialEndpointState {
            explicit MakeSharedEnabler(EndpointStateInterface& stateInterface)
                : InitialEndpointState(stateInterface) {}
        };
        return std::make_shared<MakeSharedEnabler>(stateInterface);
    }

    ~InitialEndpointState() override {
        SLogger::debug("InitialEndpointState destructor");
    }

    void close(bool /*graceful*/) override {
        SLogger::debug("InitialEndpointState::close start");
        SLogger::debug("InitialEndpointState::close end");
    }

    void send(const std::vector<std::byte>& /* data */) override {
        SLogger::debug("InitialEndpointState::send start");
        SLogger::debug("InitialEndpointState::send end");
        throw SendFailed("send() called on endpoint in initial state");
    }

    void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection) override {
        SLogger::debug("InitialEndpointState::changeToConnectingState start");

        auto self = sharedFromThis<InitialEndpointState>();
        this->stateInterface.changeToConnectingState(pendingConnection);

        SLogger::debug("InitialEndpointState::changeToConnectingState end");
    }

    void changeToConnectedState(
        const std::shared_ptr<Connection>& /*connection*/) override {
        SLogger::debug("InitialEndpointState::changeToConnectedState start");
        SLogger::debug("InitialEndpointState::changeToConnectedState end");
    }

    [[nodiscard]] bool isConnected() const override {
        SLogger::debug("InitialEndpointState::isConnected");
        return false;
    }
};

} // namespace streamr::dht::connection::endpoint

#endif // STREAMR_DHT_CONNECTION_ENDPOINT_INITIALENDPOINTSTATE_HPP