// Module streamr.dht.EndpointState
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/EndpointState.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;

#include <cstddef>
#include <memory>
#include <vector>

export module streamr.dht.EndpointState;

import streamr.logger;
import streamr.utils;
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
        const std::shared_ptr<IPendingConnection>& pendingConnection) = 0;

    virtual void changeToConnectedState(
        const std::shared_ptr<Connection>& connection) = 0;

    [[nodiscard]] virtual bool isConnected() const = 0;
};

} // namespace streamr::dht::connection::endpoint
