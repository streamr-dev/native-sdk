// Module streamr.dht.DisconnectedEndpointState
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/DisconnectedEndpointState.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <cstddef>
#include <memory>
#include <vector>

export module streamr.dht.DisconnectedEndpointState;

import streamr.logger;
import streamr.dht.Connection;
import streamr.dht.EndpointState;
import streamr.dht.EndpointStateInterface;
import streamr.dht.Errors;
import streamr.dht.IPendingConnection;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::helpers::SendFailed;

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
