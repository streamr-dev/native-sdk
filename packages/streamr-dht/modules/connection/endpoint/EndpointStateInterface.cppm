// Module streamr.dht.EndpointStateInterface
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/EndpointStateInterface.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;

#include <cstddef>
#include <memory>
#include <vector>

export module streamr.dht.EndpointStateInterface;

import streamr.dht.Connection;
import streamr.dht.IPendingConnection;
export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::IPendingConnection;

// Pure abstract callback interface through which the endpoint state
// classes drive the state machine. Endpoint implements it. Keeping this
// abstract — instead of the former concrete forwarder that held an
// Endpoint& and had its member definitions at the bottom of
// Endpoint.hpp — makes the endpoint header cluster acyclic: Endpoint
// depends on the states and the states depend only on this interface.
// (This was the only header cycle in the monorepo; the planned module
// consolidation needs the header graph to be a DAG.)
class EndpointStateInterface {
public:
    virtual ~EndpointStateInterface() = default;
    virtual void changeToConnectingState(
        const std::shared_ptr<IPendingConnection>& pendingConnection) = 0;
    virtual void changeToConnectedState(
        const std::shared_ptr<Connection>& connection) = 0;
    virtual void emitData(const std::vector<std::byte>& data) = 0;
    virtual void handleDisconnect(bool gracefulLeave) = 0;
    virtual void handleConnected() = 0;
};

} // namespace streamr::dht::connection::endpoint
