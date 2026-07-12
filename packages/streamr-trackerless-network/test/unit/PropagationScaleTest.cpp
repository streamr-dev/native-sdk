// Ported from packages/trackerless-network/test/integration/
// Propagation.test.ts (v103.8.0-rc.3): a broadcast from one node
// reaches every node of the stream-part overlay.
//
// Adaptation: the TS test runs 256 nodes; the C++ simulator carries a
// far higher per-node cost (two full DHT layers of real coroutines per
// node), and 256 nodes takes ~9-16 min locally — beyond the CI budget.
// 64 nodes exercises the same propagation logic over a real multi-hop
// mesh. Scaling this back to 256 (and the teardown stress it implies)
// is a documented follow-up.
//
// NB: NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include) — see TestUtilsTest.cpp for the clangd rationale.
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
import streamr.dht.DhtNode;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.protos;
import streamr.dht.Identifiers;
import streamr.utils.BinaryUtils;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::connection::simulator::SimulatorTransport;
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

// Local copies of the TestUtils factories: importing the TestUtils module
// on top of this TU's DhtNode + simulator + content-delivery composition
// exhausts clang's per-TU source-location space.
inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

inline StreamMessage createLocalStreamMessage(
    const std::string& content, const StreamPartID& streamPartId) {
    StreamMessage msg;
    auto* messageId = msg.mutable_messageid();
    messageId->set_streamid(StreamPartIDUtils::getStreamID(streamPartId));
    messageId->set_streampartition(
        static_cast<int32_t>(
            StreamPartIDUtils::getStreamPartition(streamPartId).value_or(0)));
    messageId->set_sequencenumber(0);
    messageId->set_timestamp(0);
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

constexpr size_t nodeCount = 64;
constexpr std::chrono::seconds meshTimeout{60};
constexpr std::chrono::seconds propagationTimeout{10};
constexpr std::chrono::milliseconds pollInterval{200};
// TS createMockContentDeliveryLayerNodeAndDhtNode DhtNode options.
constexpr size_t tsNumberOfNodesPerKBucket = 4;
constexpr size_t tsNeighborPingLimit = 16;
constexpr std::chrono::milliseconds tsRpcRequestTimeout{5000};
constexpr double averageNeighborTarget = 4.0;

struct SimNode {
    std::shared_ptr<SimulatorTransport> transport;
    std::shared_ptr<DhtNodeDiscoveryLayer> discoveryLayerNode;
    std::shared_ptr<ContentDeliveryLayerNode> contentDeliveryLayerNode;
};

SimNode createSimNode(
    const PeerDescriptor& localPeerDescriptor,
    const StreamPartID& streamPartId,
    Simulator& simulator) {
    auto transport =
        std::make_shared<SimulatorTransport>(localPeerDescriptor, simulator);
    transport->start();
    auto dhtNode = std::make_shared<DhtNode>(DhtNodeOptions{
        .serviceId = ServiceID{streamPartId},
        .numberOfNodesPerKBucket = tsNumberOfNodesPerKBucket,
        .neighborPingLimit = tsNeighborPingLimit,
        .rpcRequestTimeout = tsRpcRequestTimeout,
        .transport = transport.get(),
        .connectionsView = transport.get(),
        .connectionLocker = transport.get(),
        .peerDescriptor = localPeerDescriptor});
    auto discoveryLayerNode = std::make_shared<DhtNodeDiscoveryLayer>(dhtNode);
    auto contentDeliveryLayerNode = createContentDeliveryLayerNode(
        ContentDeliveryLayerNodeOptions{
            .streamPartId = streamPartId,
            .discoveryLayerNode = discoveryLayerNode,
            .transport = transport.get(),
            .connectionLocker = transport.get(),
            .localPeerDescriptor = localPeerDescriptor,
            .isLocalNodeEntryPoint = []() { return false; }});
    return SimNode{
        .transport = std::move(transport),
        .discoveryLayerNode = std::move(discoveryLayerNode),
        .contentDeliveryLayerNode = std::move(contentDeliveryLayerNode)};
}

} // namespace

class PropagationScaleTest : public ::testing::Test {
protected:
    StreamPartID streamPartId = StreamPartIDUtils::parse("testingtesting#0");
    PeerDescriptor entryPointDescriptor = createMockPeerDescriptor();
    Simulator simulator{LatencyType::NONE};
    std::vector<SimNode> nodes;
    std::atomic<size_t> totalReceived = 0;

    void SetUp() override {
        auto entryPoint = createSimNode(
            this->entryPointDescriptor, this->streamPartId, this->simulator);
        blockingWait(entryPoint.discoveryLayerNode->start());
        blockingWait(entryPoint.discoveryLayerNode->joinDht(
            {this->entryPointDescriptor}));
        blockingWait(entryPoint.contentDeliveryLayerNode->start());
        entryPoint.contentDeliveryLayerNode->on<Message>(
            [this](const StreamMessage& /*msg*/) { this->totalReceived++; });
        this->nodes.push_back(std::move(entryPoint));

        std::vector<folly::coro::Task<void>> joins;
        joins.reserve(nodeCount);
        for (size_t i = 0; i < nodeCount; i++) {
            auto node = createSimNode(
                createMockPeerDescriptor(),
                this->streamPartId,
                this->simulator);
            blockingWait(node.discoveryLayerNode->start());
            blockingWait(node.contentDeliveryLayerNode->start());
            node.contentDeliveryLayerNode->on<Message>(
                [this](const StreamMessage& /*msg*/) {
                    this->totalReceived++;
                });
            joins.push_back(
                node.discoveryLayerNode->joinDht({this->entryPointDescriptor}));
            this->nodes.push_back(std::move(node));
        }
        blockingWait(folly::coro::collectAllRange(std::move(joins)));
    }

    void TearDown() override {
        for (auto& node : this->nodes) {
            node.contentDeliveryLayerNode->stop();
        }
        for (auto& node : this->nodes) {
            blockingWait(node.discoveryLayerNode->stop());
        }
        for (auto& node : this->nodes) {
            node.transport->stop();
        }
        this->simulator.stop();
    }
};

TEST_F(PropagationScaleTest, AllNodesReceiveMessages) {
    blockingWait(waitForCondition(
        [this]() {
            return std::ranges::all_of(this->nodes, [](const auto& node) {
                return node.contentDeliveryLayerNode->getNeighbors().size() >=
                    3;
            });
        },
        meshTimeout,
        pollInterval));
    blockingWait(waitForCondition(
        [this]() {
            size_t sum = 0;
            for (const auto& node : this->nodes) {
                sum += node.contentDeliveryLayerNode->getNeighbors().size();
            }
            return (static_cast<double>(sum) /
                    static_cast<double>(this->nodes.size())) >=
                averageNeighborTarget;
        },
        meshTimeout,
        pollInterval));
    const auto msg =
        createLocalStreamMessage(R"({"hello":"WORLD"})", this->streamPartId);
    this->nodes[0].contentDeliveryLayerNode->broadcast(msg);
    blockingWait(waitForCondition(
        [this]() { return this->totalReceived >= nodeCount; },
        propagationTimeout,
        pollInterval));
}
