// Module streamr.dht.ConnectionsView
// CONSOLIDATED from the former header
// streamr-dht/connection/ConnectionsView.hpp (MODERNIZATION.md Phase 2.6): this
// file is now the source of truth.
module;


export module streamr.dht.ConnectionsView;

import std;

import streamr.dht.protos;

import streamr.dht.Identifiers;
export namespace streamr::dht::connection {

using ::dht::PeerDescriptor;

class ConnectionsView {
public:
    virtual ~ConnectionsView() = default;

    [[nodiscard]] virtual std::vector<PeerDescriptor> getConnections() = 0;
    [[nodiscard]] virtual std::size_t getConnectionCount() = 0;
    [[nodiscard]] virtual bool hasConnection(const DhtAddress& nodeId) = 0;

protected:
    ConnectionsView() = default;
};

} // namespace streamr::dht::connection
