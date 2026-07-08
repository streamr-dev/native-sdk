// Module streamr.dht.InitialEndpointState
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/InitialEndpointState.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;
#include <new>


export module streamr.dht.InitialEndpointState;

import std;

import streamr.logger.SLogger;
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

// All methods are called with the state-machine mutex held; see
// EndpointState for the contract.
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

    [[nodiscard]] DeferredCallout close(bool /*graceful*/) override {
        SLogger::debug("InitialEndpointState::close");
        return {};
    }

    void send(const std::vector<std::byte>& /* data */) override {
        SLogger::debug("InitialEndpointState::send");
        throw SendFailed("send() called on endpoint in initial state");
    }

    [[nodiscard]] DeferredCallout changeToConnectingState(
        const std::shared_ptr<IPendingConnection>& pendingConnection) override {
        SLogger::debug("InitialEndpointState::changeToConnectingState");
        this->stateInterface.enterConnectingState(pendingConnection);
        return {};
    }

    [[nodiscard]] DeferredCallout changeToConnectedState(
        const std::shared_ptr<Connection>& /*connection*/) override {
        SLogger::debug("InitialEndpointState::changeToConnectedState");
        return {};
    }

    [[nodiscard]] bool isConnected() const override {
        SLogger::debug("InitialEndpointState::isConnected");
        return false;
    }
};

} // namespace streamr::dht::connection::endpoint
