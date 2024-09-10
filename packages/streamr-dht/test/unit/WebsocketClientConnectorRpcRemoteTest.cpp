#include "streamr-dht/connection/websocket/WebsocketClientConnectorRpcRemote.hpp"
#include "packages/dht/protos/DhtRpc.client.pb.h"
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"

#include <gtest/gtest.h>

using ::dht::PeerDescriptor;
using ::dht::WebsocketClientConnectorRpcClient;
using streamr::dht::connection::websocket::WebsocketClientConnectorRpcRemote;
using streamr::dht::contact::DhtCallContext;
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
