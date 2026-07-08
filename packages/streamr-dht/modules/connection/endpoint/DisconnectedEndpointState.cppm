// Module streamr.dht.DisconnectedEndpointState
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/DisconnectedEndpointState.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;
#include <new> // operator new ambiguity under import std (local-type container allocation) — see convert-to-import-std.py


export module streamr.dht.DisconnectedEndpointState;

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

// Terminal state: a disconnected endpoint ignores further transitions
// (a connection accepted for it by the simultaneous-connect tiebreak
// while the disconnect was completing is deliberately dropped). All
// methods are called with the state-machine mutex held; see
// EndpointState for the contract.
class DisconnectedEndpointState : public EndpointState {
private:
    // The terminal state drives no further transitions, so unlike its
    // siblings it does not keep the EndpointStateInterface reference.
    explicit DisconnectedEndpointState(
        EndpointStateInterface& /*stateInterface*/) {
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

    [[nodiscard]] DeferredCallout close(bool /*graceful*/) override {
        SLogger::debug("DisconnectedEndpointState::close");
        return {};
    }

    void send(const std::vector<std::byte>& /* data */) override {
        SLogger::debug("DisconnectedEndpointState::send");
        throw SendFailed("send() called on disconnected endpoint");
    }

    [[nodiscard]] DeferredCallout changeToConnectingState(
        const std::shared_ptr<IPendingConnection>& /*pendingConnection*/)
        override {
        SLogger::debug("DisconnectedEndpointState::changeToConnectingState");
        return {};
    }

    [[nodiscard]] DeferredCallout changeToConnectedState(
        const std::shared_ptr<Connection>& /*connection*/) override {
        SLogger::debug("DisconnectedEndpointState::changeToConnectedState");
        return {};
    }

    [[nodiscard]] bool isConnected() const override {
        SLogger::debug("DisconnectedEndpointState::isConnected");
        return false;
    }
};

} // namespace streamr::dht::connection::endpoint
