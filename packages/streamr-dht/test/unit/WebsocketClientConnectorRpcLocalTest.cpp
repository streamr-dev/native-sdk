#include <gtest/gtest.h>
#include "streamr-dht/connection/websocket/WebsocketClientConnectorRpcLocal.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"

using streamr::dht::connection::websocket::WebsocketClientConnectorRpcLocal;
using streamr::dht::connection::websocket::WebsocketClientConnectorRpcLocalOptions;
using ::dht::PeerDescriptor;
using streamr::dht::connection::PendingConnection;
using streamr::dht::DhtAddress;

TEST(WebsocketClientConnectorRpcLocal, TestCanBeCreated) {
    WebsocketClientConnectorRpcLocalOptions options{
        .connect = [](const PeerDescriptor& /*peer*/) {
            return PendingConnection();
        },
        .hasConnection = [](const DhtAddress& /*nodeId*/) {
            return false;
        },
        .onNewConnection = [](const PendingConnection& /*connection*/) {
            return true;
        }
    };
    WebsocketClientConnectorRpcLocal connectorLocal(std::move(options));
}