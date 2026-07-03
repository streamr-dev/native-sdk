#include <gtest/gtest.h>

import streamr.dht;
import streamr.protorpc;

using ::dht::PeerDescriptor;
using ::dht::WebsocketClientConnectorRpcClient;
using streamr::dht::connection::websocket::WebsocketClientConnectorRpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::protorpc::RpcCommunicator;

TEST(WebsocketClientConnectorRpcRemoteTest, ItCanBeCreated) {
    RpcCommunicator<DhtCallContext> communicator;
    PeerDescriptor localPeerDescriptor;
    PeerDescriptor remotePeerDescriptor;
    WebsocketClientConnectorRpcClient client(communicator);

    WebsocketClientConnectorRpcRemote connector(
        std::move(localPeerDescriptor),
        std::move(remotePeerDescriptor),
        std::move(client)); // NOLINT
}
