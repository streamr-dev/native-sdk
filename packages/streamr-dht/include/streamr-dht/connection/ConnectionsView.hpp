#ifndef STREAMR_DHT_CONNECTION_CONNECTIONSVIEW_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONSVIEW_HPP

#include "streamr-dht/Identifiers.hpp"
#include "packages/dht/protos/DhtRpc.pb.h"

namespace streamr::dht::connection {

using ::dht::PeerDescriptor;

class ConnectionsView {
public:
    virtual ~ConnectionsView() = default;

    [[nodiscard]] virtual std::vector<PeerDescriptor> getConnections() const = 0;
    [[nodiscard]] virtual size_t getConnectionCount() const = 0;
    [[nodiscard]] virtual bool hasConnection(const DhtAddress& nodeId) const = 0;

protected:
    ConnectionsView() = default;
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_CONNECTIONSVIEW_HPP