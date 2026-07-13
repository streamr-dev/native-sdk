// Ported from packages/trackerless-network/test/end-to-end/
// external-network-rpc.test.ts (v103.8.0-rc.3): an application registers
// a custom RPC method on the server node's external-network service and
// a client node queries it over a real websocket connection.
//
// NB: TestUtils and the textual pb.h are avoided — this TU composes the
// full NetworkNode + NetworkStack + DhtNode module graph and additional
// BMIs exhaust clang's per-TU source-location space (see the C3/C5 test
// files).
#include <memory>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.NetworkNode;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.protos;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::trackerlessnetwork::NetworkNode;
using streamr::trackerlessnetwork::NetworkOptions;
using streamr::trackerlessnetwork::NetworkStack;
using streamr::utils::blockingWait;
using HandshakeRpcClient =
    streamr::protorpc::HandshakeRpcClient<DhtCallContext>;

namespace {

constexpr uint16_t serverPort = 15499;

inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

} // namespace

class ExternalNetworkRpcTest : public ::testing::Test {
protected:
    PeerDescriptor serverPeerDescriptor;
    PeerDescriptor clientPeerDescriptor = createMockPeerDescriptor();
    std::shared_ptr<NetworkNode> clientNode;
    std::shared_ptr<NetworkNode> serverNode;

    void SetUp() override {
        this->serverPeerDescriptor = createMockPeerDescriptor();
        this->serverPeerDescriptor.mutable_websocket()->set_host("127.0.0.1");
        this->serverPeerDescriptor.mutable_websocket()->set_port(serverPort);
        this->serverPeerDescriptor.mutable_websocket()->set_tls(false);

        this->clientNode = std::make_shared<NetworkNode>(
            std::make_shared<NetworkStack>(NetworkOptions{
                .layer0 = DhtNodeOptions{
                    .peerDescriptor = this->clientPeerDescriptor,
                    .entryPoints = {this->serverPeerDescriptor}}}));
        this->serverNode = std::make_shared<NetworkNode>(
            std::make_shared<NetworkStack>(NetworkOptions{
                .layer0 = DhtNodeOptions{
                    .peerDescriptor = this->serverPeerDescriptor,
                    .entryPoints = {this->serverPeerDescriptor}}}));

        blockingWait(this->serverNode->start());
        blockingWait(this->clientNode->start());
    }

    void TearDown() override {
        if (this->serverNode) {
            blockingWait(this->serverNode->stop());
        }
        if (this->clientNode) {
            blockingWait(this->clientNode->stop());
        }
    }
};

TEST_F(ExternalNetworkRpcTest, CanMakeQueries) {
    const std::string requestId = "TEST";
    this->serverNode->registerExternalNetworkRpcMethod<
        StreamPartHandshakeRequest,
        StreamPartHandshakeResponse>(
        "handshake",
        [&requestId](
            const StreamPartHandshakeRequest& /* request */,
            const DhtCallContext& /* context */) {
            StreamPartHandshakeResponse response;
            response.set_requestid(requestId);
            return response;
        });
    auto client =
        this->clientNode->createExternalRpcClient<HandshakeRpcClient>();
    DhtCallContext context;
    context.sourceDescriptor = this->clientPeerDescriptor;
    context.targetDescriptor = this->serverPeerDescriptor;
    const auto response = blockingWait(client.handshake(
        StreamPartHandshakeRequest{}, std::move(context), std::nullopt));
    EXPECT_EQ(response.requestid(), requestId);
}
