#ifndef STREAMR_DHT_CONNECTION_ENDPOINT_ENDPOINTSTATEINTERFACE_HPP
#define STREAMR_DHT_CONNECTION_ENDPOINT_ENDPOINTSTATEINTERFACE_HPP

#include <cstddef>
#include <memory>
#include <vector>
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-dht/connection/endpoint/EndpointState.hpp"

namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::IPendingConnection;

class Endpoint;
class EndpointState;

class EndpointStateInterface {
private:
    Endpoint& endpoint;

public:
    explicit EndpointStateInterface(Endpoint& ep);
    void changeToConnectingState(
        const std::shared_ptr<IPendingConnection>& pendingConnection);
    void changeToConnectedState(const std::shared_ptr<Connection>& connection);
    void emitData(const std::vector<std::byte>& data);
    void handleDisconnect(bool gracefulLeave);
    void handleConnected();
};

} // namespace streamr::dht::connection::endpoint

#endif // STREAMR_DHT_CONNECTION_ENDPOINT_ENDPOINTSTATEINTERFACE_HPP