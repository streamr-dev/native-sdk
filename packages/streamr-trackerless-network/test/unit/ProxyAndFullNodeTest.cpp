// Ported from packages/trackerless-network/test/end-to-end/
// proxy-and-full-node.test.ts (v103.8.0-rc.3, deferred from phase C5):
// a node with only proxy connections on one stream part still acts as a
// FULL node on other stream parts (joining the control layer on first
// non-proxied broadcast).
//
// NB: TestUtils and the textual pb.h are avoided — this TU composes the
// full NetworkNode + NetworkStack + DhtNode module graph and additional
// BMIs exhaust clang's per-TU source-location space (see the C3/C5 test
// files).
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryManager;
import streamr.trackerlessnetwork.NetworkNode;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.utils.BinaryUtils;
import streamr.utils.EthereumAddress;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::trackerlessnetwork::ContentDeliveryManagerOptions;
using streamr::trackerlessnetwork::createNetworkNode;
using streamr::trackerlessnetwork::NetworkNode;
using streamr::trackerlessnetwork::NetworkOptions;
using streamr::trackerlessnetwork::contentdeliverymanagerevents::NewMessage;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::EthereumAddress;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::toEthereumAddress;
using streamr::utils::waitForCondition;

namespace {

constexpr uint16_t proxyPort = 23135;
constexpr auto defaultTimeout = std::chrono::seconds(10);

inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

StreamMessage createTestMessage(const StreamPartID& streamPartId) {
    StreamMessage msg;
    auto* messageId = msg.mutable_messageid();
    messageId->set_streamid(StreamPartIDUtils::getStreamID(streamPartId));
    messageId->set_streampartition(
        static_cast<int32_t>(
            StreamPartIDUtils::getStreamPartition(streamPartId).value_or(0)));
    messageId->set_timestamp(666); // NOLINT
    messageId->set_sequencenumber(0);
    messageId->set_publisherid(
        BinaryUtils::hexToBinaryString(
            "0x1234567890123456789012345678901234567890"));
    messageId->set_messagechainid("msgChainId");
    auto* previousMessageRef = msg.mutable_previousmessageref();
    previousMessageRef->set_timestamp(665); // NOLINT
    previousMessageRef->set_sequencenumber(0);
    auto* contentMessage = msg.mutable_contentmessage();
    contentMessage->set_content(R"({ "hello": "world" })");
    contentMessage->set_contenttype(ContentType::JSON);
    contentMessage->set_encryptiontype(EncryptionType::NONE);
    msg.set_signaturetype(SignatureType::ECDSA_SECP256K1_EVM);
    msg.set_signature(BinaryUtils::hexToBinaryString("0x1234"));
    return msg;
}

} // namespace

class ProxyAndFullNodeTest : public ::testing::Test {
protected:
    StreamPartID proxiedStreamPart = StreamPartIDUtils::parse("proxy-stream#0");
    StreamPartID regularStreamPart1 =
        StreamPartIDUtils::parse("regular-stream1#0");
    StreamPartID regularStreamPart2 =
        StreamPartIDUtils::parse("regular-stream2#0");
    StreamPartID regularStreamPart3 =
        StreamPartIDUtils::parse("regular-stream3#0");
    StreamPartID regularStreamPart4 =
        StreamPartIDUtils::parse("regular-stream4#0");
    EthereumAddress proxiedNodeUserId =
        toEthereumAddress("0x3333333333333333333333333333333333333333");
    std::shared_ptr<NetworkNode> proxyNode;
    std::shared_ptr<NetworkNode> proxiedNode;

    void SetUp() override {
        auto proxyNodeDescriptor = createMockPeerDescriptor();
        proxyNodeDescriptor.mutable_websocket()->set_host("127.0.0.1");
        proxyNodeDescriptor.mutable_websocket()->set_port(proxyPort);
        proxyNodeDescriptor.mutable_websocket()->set_tls(false);
        const auto proxiedNodeDescriptor = createMockPeerDescriptor();

        this->proxyNode = createNetworkNode(
            NetworkOptions{
                .layer0 =
                    DhtNodeOptions{
                        .peerDescriptor = proxyNodeDescriptor,
                        .entryPoints = {proxyNodeDescriptor}},
                .networkNode = ContentDeliveryManagerOptions{
                    .acceptProxyConnections = true}});
        blockingWait(this->proxyNode->start());
        auto& proxyManager =
            this->proxyNode->getStack().getContentDeliveryManager();
        proxyManager.joinStreamPart(proxiedStreamPart);
        proxyManager.joinStreamPart(regularStreamPart1);
        proxyManager.joinStreamPart(regularStreamPart2);
        proxyManager.joinStreamPart(regularStreamPart3);
        proxyManager.joinStreamPart(regularStreamPart4);

        this->proxiedNode = createNetworkNode(
            NetworkOptions{
                .layer0 = DhtNodeOptions{
                    .peerDescriptor = proxiedNodeDescriptor,
                    .entryPoints = {proxyNodeDescriptor}}});
        blockingWait(this->proxiedNode->start(false));
    }

    // Null-guarded: when SetUp() throws (e.g. a websocket port is
    // still in TIME_WAIT), gtest still runs TearDown() with later
    // members never assigned.
    void TearDown() override {
        if (this->proxyNode) {
            blockingWait(this->proxyNode->stop());
        }
        if (this->proxiedNode) {
            blockingWait(this->proxiedNode->stop());
        }
    }

    void broadcastAndWaitForMessage(const StreamPartID& streamPartId) {
        std::atomic<bool> received{false};
        auto& proxyManager =
            this->proxyNode->getStack().getContentDeliveryManager();
        const auto token = proxyManager.on<NewMessage>(
            [&received, &streamPartId](const StreamMessage& msg) {
                if (msg.messageid().streamid() ==
                    StreamPartIDUtils::getStreamID(streamPartId)) {
                    received = true;
                }
            });
        blockingWait(
            this->proxiedNode->broadcast(createTestMessage(streamPartId)));
        blockingWait(waitForCondition(
            [&received]() { return received.load(); }, defaultTimeout));
        proxyManager.off<NewMessage>(token);
    }
};

TEST_F(ProxyAndFullNodeTest, ProxiedNodeCanActAsFullNodeOnAnotherStreamPart) {
    blockingWait(this->proxiedNode->setProxies(
        proxiedStreamPart,
        {this->proxyNode->getPeerDescriptor()},
        std::nullopt,
        this->proxiedNodeUserId));
    EXPECT_FALSE(
        this->proxiedNode->getStack().getControlLayerNode().hasJoined());

    this->broadcastAndWaitForMessage(regularStreamPart1);

    EXPECT_TRUE(
        this->proxiedNode->getStack().getControlLayerNode().hasJoined());

    this->broadcastAndWaitForMessage(proxiedStreamPart);

    auto& proxiedManager =
        this->proxiedNode->getStack().getContentDeliveryManager();
    EXPECT_TRUE(
        proxiedManager.getStreamPartDelivery(proxiedStreamPart)->proxied);
    EXPECT_FALSE(
        proxiedManager.getStreamPartDelivery(regularStreamPart1)->proxied);
}

TEST_F(ProxyAndFullNodeTest, ProxiedNodeCanActAsFullNodeOnMultipleStreamParts) {
    blockingWait(this->proxiedNode->setProxies(
        proxiedStreamPart,
        {this->proxyNode->getPeerDescriptor()},
        std::nullopt,
        this->proxiedNodeUserId));
    EXPECT_FALSE(
        this->proxiedNode->getStack().getControlLayerNode().hasJoined());

    this->broadcastAndWaitForMessage(regularStreamPart1);
    this->broadcastAndWaitForMessage(regularStreamPart2);
    this->broadcastAndWaitForMessage(regularStreamPart3);
    this->broadcastAndWaitForMessage(regularStreamPart4);

    EXPECT_TRUE(
        this->proxiedNode->getStack().getControlLayerNode().hasJoined());

    this->broadcastAndWaitForMessage(proxiedStreamPart);

    auto& proxiedManager =
        this->proxiedNode->getStack().getContentDeliveryManager();
    EXPECT_TRUE(
        proxiedManager.getStreamPartDelivery(proxiedStreamPart)->proxied);
    EXPECT_FALSE(
        proxiedManager.getStreamPartDelivery(regularStreamPart1)->proxied);
    EXPECT_FALSE(
        proxiedManager.getStreamPartDelivery(regularStreamPart2)->proxied);
    EXPECT_FALSE(
        proxiedManager.getStreamPartDelivery(regularStreamPart3)->proxied);
    EXPECT_FALSE(
        proxiedManager.getStreamPartDelivery(regularStreamPart4)->proxied);
}
