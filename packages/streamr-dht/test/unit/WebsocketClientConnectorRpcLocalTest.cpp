#include <gtest/gtest.h>

import streamr.dht.IPendingConnection;
import streamr.dht.Identifiers;
import streamr.dht.PendingConnection;
import streamr.dht.WebsocketClientConnectorRpcLocal;
import streamr.dht.protos;
import streamr.utils.AbortController;

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::connection::IPendingConnection;
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
                return PendingConnection::newInstance(peer);
            },
        .hasConnection = [](const DhtAddress& /*nodeId*/) { return false; },
        .onNewConnection =
            [](const std::shared_ptr<IPendingConnection>& /*connection*/) {
                return true;
            },
        .abortSignal = abortController.getSignal()};
    WebsocketClientConnectorRpcLocal connectorLocal(std::move(options));
}
