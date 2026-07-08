// Module streamr.dht.EndpointState
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/EndpointState.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;
#include <new>


export module streamr.dht.EndpointState;

import std;

import streamr.logger.SLogger;
import streamr.utils.EnableSharedFromThis;
import streamr.dht.Connection;
import streamr.dht.IPendingConnection;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;
export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::IPendingConnection;

// LOCKING CONTRACT (phase A0, see EndpointStateInterface): every method
// here is called by Endpoint with the state-machine mutex held. Methods
// that need to perform a call-out (closing a connection or a pending
// connection — both can emit events synchronously) must not do it
// inline; they return it as a DeferredCallout, which Endpoint runs after
// releasing the mutex.
class EndpointState : public EnableSharedFromThis {
protected:
    EndpointState() { SLogger::debug("EndpointState constructor"); }

public:
    using DeferredCallout = std::function<void()>;

    ~EndpointState() override { SLogger::debug("EndpointState destructor"); }

    [[nodiscard]] virtual DeferredCallout close(bool /*graceful*/) {
        SLogger::debug("EndpointState::close");
        return {};
    };
    virtual void send(const std::vector<std::byte>& /* data */) {
        SLogger::debug("EndpointState::send");
    };

    [[nodiscard]] virtual DeferredCallout changeToConnectingState(
        const std::shared_ptr<IPendingConnection>& pendingConnection) = 0;

    [[nodiscard]] virtual DeferredCallout changeToConnectedState(
        const std::shared_ptr<Connection>& connection) = 0;

    [[nodiscard]] virtual bool isConnected() const = 0;
};

} // namespace streamr::dht::connection::endpoint
