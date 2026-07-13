// Ported from packages/trackerless-network/test/end-to-end/
// content-delivery-layer-node-with-real-connections.test.ts
// (v103.8.0-rc.3): five ContentDeliveryLayerNodes over layer-0 DhtNodes
// that talk REAL websockets — the TS comment applies here too: the
// layer-0 nodes act as the per-stream discovery layers directly
// (wrapped in DhtNodeDiscoveryLayer for the nominal type).
//
// NB: TestUtils and the textual pb.h are avoided — this TU composes the
// full DhtNode + content-delivery module graph and additional BMIs
// exhaust clang's per-TU source-location space (see the C3/C5 test
// files).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryLayerNode;
import streamr.trackerlessnetwork.createContentDeliveryLayerNode;
import streamr.trackerlessnetwork.DhtNodeDiscoveryLayer;
import streamr.trackerlessnetwork.protos;
import streamr.dht.ConnectionLocker;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.PortRange;
import streamr.dht.protos;
import streamr.utils.BinaryUtils;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::types::PortRange;
using streamr::trackerlessnetwork::ContentDeliveryLayerNode;
using streamr::trackerlessnetwork::ContentDeliveryLayerNodeOptions;
using streamr::trackerlessnetwork::createContentDeliveryLayerNode;
using streamr::trackerlessnetwork::contentdeliverylayernodeevents::Message;
using streamr::trackerlessnetwork::discoverylayer::DhtNodeDiscoveryLayer;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::waitForCondition;

namespace {

constexpr uint16_t entryPointPort = 12221;
constexpr PortRange websocketPortRange{.min = 12222, .max = 12225};
constexpr auto topologyTimeout = std::chrono::seconds(10);

inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

StreamMessage createTestMessage(
    const std::string& content, const StreamPartID& streamPartId) {
    StreamMessage msg;
    auto* messageId = msg.mutable_messageid();
    messageId->set_streamid(StreamPartIDUtils::getStreamID(streamPartId));
    messageId->set_streampartition(
        static_cast<int32_t>(
            StreamPartIDUtils::getStreamPartition(streamPartId).value_or(0)));
    messageId->set_sequencenumber(0);
    messageId->set_timestamp(666); // NOLINT
    messageId->set_publisherid(
        BinaryUtils::hexToBinaryString(
            "0x1234567890123456789012345678901234567890"));
    messageId->set_messagechainid("messageChain0");
    msg.set_signaturetype(SignatureType::ECDSA_SECP256K1_EVM);
    msg.set_signature(BinaryUtils::hexToBinaryString("0x1234"));
    auto* contentMessage = msg.mutable_contentmessage();
    contentMessage->set_encryptiontype(EncryptionType::NONE);
    contentMessage->set_contenttype(ContentType::JSON);
    contentMessage->set_content(content);
    return msg;
}

} // namespace

class ContentDeliveryLayerNodeRealConnectionsTest : public ::testing::Test {
protected:
    StreamPartID streamPartId = StreamPartIDUtils::parse("random-graph#0");
    std::vector<std::shared_ptr<DhtNode>> dhtNodes;
    std::vector<std::shared_ptr<ContentDeliveryLayerNode>>
        contentDeliveryLayerNodes;

    void SetUp() override {
        auto epPeerDescriptor = createMockPeerDescriptor();
        epPeerDescriptor.mutable_websocket()->set_host("127.0.0.1");
        epPeerDescriptor.mutable_websocket()->set_port(entryPointPort);
        epPeerDescriptor.mutable_websocket()->set_tls(false);

        auto epDhtNode = std::make_shared<DhtNode>(
            DhtNodeOptions{.peerDescriptor = epPeerDescriptor});
        blockingWait(epDhtNode->start());
        this->dhtNodes.push_back(epDhtNode);
        for (size_t i = 0; i < 4; i++) {
            auto node = std::make_shared<DhtNode>(DhtNodeOptions{
                .entryPoints = {epPeerDescriptor},
                .websocketPortRange = websocketPortRange});
            blockingWait(node->start());
            this->dhtNodes.push_back(node);
        }

        for (const auto& dhtNode : this->dhtNodes) {
            auto node = createContentDeliveryLayerNode(
                ContentDeliveryLayerNodeOptions{
                    .streamPartId = this->streamPartId,
                    .discoveryLayerNode =
                        std::make_shared<DhtNodeDiscoveryLayer>(dhtNode),
                    .transport = dhtNode->getTransport(),
                    .connectionLocker = dhtNode->getConnectionLocker(),
                    .localPeerDescriptor = dhtNode->getLocalPeerDescriptor(),
                    .isLocalNodeEntryPoint = []() { return false; }});
            blockingWait(node->start());
            this->contentDeliveryLayerNodes.push_back(node);
        }

        blockingWait(this->dhtNodes[0]->joinDht({epPeerDescriptor}));
        std::vector<folly::coro::Task<void>> joins;
        for (size_t i = 1; i < this->dhtNodes.size(); i++) {
            joins.push_back(this->dhtNodes[i]->joinDht({epPeerDescriptor}));
        }
        blockingWait(folly::coro::collectAllRange(std::move(joins)));
    }

    // Null-guarded like the other end-to-end fixtures.
    void TearDown() override {
        for (const auto& node : this->contentDeliveryLayerNodes) {
            if (node) {
                node->stop();
            }
        }
        for (const auto& dhtNode : this->dhtNodes) {
            if (dhtNode) {
                dhtNode->stop();
            }
        }
    }

    [[nodiscard]] bool allHaveAtLeastThreeNeighbors() const {
        return std::ranges::all_of(
            this->contentDeliveryLayerNodes,
            [](const auto& node) { return node->getNeighbors().size() >= 3; });
    }
};

TEST_F(ContentDeliveryLayerNodeRealConnectionsTest, FullyConnectedTopologies) {
    blockingWait(waitForCondition(
        [this]() { return this->allHaveAtLeastThreeNeighbors(); },
        topologyTimeout));
    for (const auto& node : this->contentDeliveryLayerNodes) {
        EXPECT_GE(node->getNeighbors().size(), 3);
    }
}

TEST_F(ContentDeliveryLayerNodeRealConnectionsTest, CanPropagateMessages) {
    std::atomic<size_t> receivedMessageCount{0};
    for (size_t i = 1; i < this->contentDeliveryLayerNodes.size(); i++) {
        this->contentDeliveryLayerNodes[i]->on<Message>(
            [&receivedMessageCount](const StreamMessage& /* msg */) {
                receivedMessageCount += 1;
            });
    }
    blockingWait(waitForCondition(
        [this]() { return this->allHaveAtLeastThreeNeighbors(); },
        topologyTimeout));

    const auto msg =
        createTestMessage(R"({ "hello": "WORLD" })", this->streamPartId);
    this->contentDeliveryLayerNodes[0]->broadcast(msg);
    blockingWait(waitForCondition([&receivedMessageCount]() {
        return receivedMessageCount.load() >= 4;
    }));
}
