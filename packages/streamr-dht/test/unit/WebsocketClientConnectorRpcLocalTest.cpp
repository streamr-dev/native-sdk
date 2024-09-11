#include "streamr-dht/connection/websocket/WebsocketClientConnectorRpcLocal.hpp"
#include <gtest/gtest.h>
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-utils/AbortController.hpp"
using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::websocket::WebsocketClientConnectorRpcLocal;
using streamr::dht::connection::websocket::
    WebsocketClientConnectorRpcLocalOptions;
using streamr::utils::AbortController;

TEST(WebsocketClientConnectorRpcLocal, TestCanBeCreated) {
    AbortController abortController;
    WebsocketClientConnectorRpcLocalOptions options{
        .connect =
            [](const PeerDescriptor& peer) {
                return std::make_shared<PendingConnection>(peer);
            },
        .hasConnection = [](const DhtAddress& /*nodeId*/) { return false; },
        .onNewConnection =
            [](const std::shared_ptr<PendingConnection>& /*connection*/) {
                return true;
            },
        .abortSignal = abortController.getSignal()};
    WebsocketClientConnectorRpcLocal connectorLocal(std::move(options));
}