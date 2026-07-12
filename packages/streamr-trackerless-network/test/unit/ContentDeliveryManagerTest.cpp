// Ported from packages/trackerless-network/test/integration/
// ContentDeliveryManager.test.ts (v103.8.0-rc.3): two managers over
// layer-0 DhtNodes on a simulator — joining, pub/sub, multi-stream,
// leaving, and RTT collection.
//
// NB: TestUtils and the textual pb.h are avoided — this TU composes the
// full DhtNode + manager module graph and additional BMIs exhaust
// clang's per-TU source-location space (see the C3 test files).
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryManager;
import streamr.trackerlessnetwork.DhtNodeControlLayer;
import streamr.trackerlessnetwork.createStreamPartDiscoveryLayerNode;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.protos;
import streamr.utils.BinaryUtils;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::connection::simulator::SimulatorTransport;
using streamr::trackerlessnetwork::ContentDeliveryManager;
using streamr::trackerlessnetwork::ContentDeliveryManagerOptions;
using streamr::trackerlessnetwork::contentdeliverymanagerevents::NewMessage;
using streamr::trackerlessnetwork::controllayer::
    createStreamPartDiscoveryLayerNode;
using streamr::trackerlessnetwork::controllayer::DhtNodeControlLayer;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::waitForCondition;

namespace {

constexpr std::chrono::milliseconds neighborUpdateInterval{100};
constexpr std::chrono::seconds untilTimeout{15};
constexpr std::chrono::milliseconds pollInterval{100};

// Local copies of the TestUtils factories (see the NB above).
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

class ContentDeliveryManagerTest : public ::testing::Test {
protected:
    PeerDescriptor peerDescriptor1 = createMockPeerDescriptor();
    PeerDescriptor peerDescriptor2 = createMockPeerDescriptor();
    StreamPartID streamPartId = StreamPartIDUtils::parse("test#0");
    Simulator simulator{LatencyType::NONE};
    std::shared_ptr<SimulatorTransport> transport1;
    std::shared_ptr<SimulatorTransport> transport2;
    std::shared_ptr<DhtNodeControlLayer> controlLayerNode1;
    std::shared_ptr<DhtNodeControlLayer> controlLayerNode2;
    std::shared_ptr<ContentDeliveryManager> manager1;
    std::shared_ptr<ContentDeliveryManager> manager2;

    void SetUp() override {
        this->transport1 = std::make_shared<SimulatorTransport>(
            this->peerDescriptor1, this->simulator);
        this->transport1->start();
        this->transport2 = std::make_shared<SimulatorTransport>(
            this->peerDescriptor2, this->simulator);
        this->transport2->start();
        this->controlLayerNode1 = std::make_shared<DhtNodeControlLayer>(
            std::make_shared<DhtNode>(DhtNodeOptions{
                .transport = this->transport1.get(),
                .connectionsView = this->transport1.get(),
                .peerDescriptor = this->peerDescriptor1,
                .entryPoints = {this->peerDescriptor1}}));
        this->controlLayerNode2 = std::make_shared<DhtNodeControlLayer>(
            std::make_shared<DhtNode>(DhtNodeOptions{
                .transport = this->transport2.get(),
                .connectionsView = this->transport2.get(),
                .peerDescriptor = this->peerDescriptor2,
                .entryPoints = {this->peerDescriptor1}}));
        blockingWait(
            folly::coro::collectAll(
                this->controlLayerNode1->start(),
                this->controlLayerNode2->start()));
        blockingWait(
            folly::coro::collectAll(
                this->controlLayerNode1->joinDht({this->peerDescriptor1}),
                this->controlLayerNode2->joinDht({this->peerDescriptor1})));

        this->manager1 = std::make_shared<ContentDeliveryManager>(
            ContentDeliveryManagerOptions{
                .neighborUpdateInterval = neighborUpdateInterval,
                .createDiscoveryLayerNode =
                    [this](
                        const StreamPartID& id,
                        std::vector<PeerDescriptor> entryPoints) {
                        return createStreamPartDiscoveryLayerNode(
                            id,
                            std::move(entryPoints),
                            *this->controlLayerNode1);
                    }});
        this->manager2 = std::make_shared<ContentDeliveryManager>(
            ContentDeliveryManagerOptions{
                .neighborUpdateInterval = neighborUpdateInterval,
                .createDiscoveryLayerNode =
                    [this](
                        const StreamPartID& id,
                        std::vector<PeerDescriptor> entryPoints) {
                        return createStreamPartDiscoveryLayerNode(
                            id,
                            std::move(entryPoints),
                            *this->controlLayerNode2);
                    }});
        blockingWait(this->manager1->start(
            *this->controlLayerNode1, *this->transport1, *this->transport1));
        this->manager1->setStreamPartEntryPoints(
            this->streamPartId, {this->peerDescriptor1});
        blockingWait(this->manager2->start(
            *this->controlLayerNode2, *this->transport2, *this->transport2));
        this->manager2->setStreamPartEntryPoints(
            this->streamPartId, {this->peerDescriptor1});
    }

    void TearDown() override {
        blockingWait(this->manager1->destroy());
        blockingWait(this->manager2->destroy());
        blockingWait(this->controlLayerNode1->stop());
        blockingWait(this->controlLayerNode2->stop());
        this->transport1->stop();
        this->transport2->stop();
        this->simulator.stop();
    }

    void joinBothAndAwaitNeighbors() {
        this->manager1->joinStreamPart(this->streamPartId);
        this->manager2->joinStreamPart(this->streamPartId);
        blockingWait(waitForCondition(
            [this]() {
                return this->manager1->getNeighbors(this->streamPartId)
                           .size() == 1 &&
                    this->manager2->getNeighbors(this->streamPartId).size() ==
                    1;
            },
            untilTimeout,
            pollInterval));
    }
};

TEST_F(ContentDeliveryManagerTest, Starts) {
    EXPECT_EQ(
        Identifiers::getNodeIdFromPeerDescriptor(
            this->manager1->getPeerDescriptor()),
        Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor1));
    EXPECT_EQ(
        Identifiers::getNodeIdFromPeerDescriptor(
            this->manager2->getPeerDescriptor()),
        Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor2));
}

TEST_F(ContentDeliveryManagerTest, JoiningStream) {
    this->joinBothAndAwaitNeighbors();
    EXPECT_EQ(this->manager1->getNeighbors(this->streamPartId).size(), 1);
    EXPECT_EQ(this->manager2->getNeighbors(this->streamPartId).size(), 1);
}

TEST_F(
    ContentDeliveryManagerTest, PublishingAfterJoiningAndWaitingForNeighbors) {
    this->joinBothAndAwaitNeighbors();
    std::atomic<size_t> received1 = 0;
    this->manager1->on<NewMessage>(
        [&received1](const StreamMessage& /*msg*/) { received1++; });
    this->manager2->broadcast(
        createLocalStreamMessage(R"({"hello":"WORLD"})", this->streamPartId));
    blockingWait(waitForCondition(
        [&received1]() { return received1 >= 1; }, untilTimeout, pollInterval));
}

TEST_F(ContentDeliveryManagerTest, MultiStreamPubSub) {
    const auto streamPartId2 = StreamPartIDUtils::parse("test2#0");
    this->manager1->setStreamPartEntryPoints(
        streamPartId2, {this->peerDescriptor1});
    this->manager2->setStreamPartEntryPoints(
        streamPartId2, {this->peerDescriptor1});
    this->manager1->joinStreamPart(this->streamPartId);
    this->manager1->joinStreamPart(streamPartId2);
    this->manager2->joinStreamPart(this->streamPartId);
    this->manager2->joinStreamPart(streamPartId2);
    blockingWait(waitForCondition(
        [this, &streamPartId2]() {
            return this->manager1->getNeighbors(this->streamPartId).size() ==
                1 &&
                this->manager2->getNeighbors(this->streamPartId).size() == 1 &&
                this->manager1->getNeighbors(streamPartId2).size() == 1 &&
                this->manager2->getNeighbors(streamPartId2).size() == 1;
        },
        untilTimeout,
        pollInterval));
    std::atomic<size_t> received1 = 0;
    std::atomic<size_t> received2 = 0;
    this->manager1->on<NewMessage>(
        [&received1](const StreamMessage& /*msg*/) { received1++; });
    this->manager2->on<NewMessage>(
        [&received2](const StreamMessage& /*msg*/) { received2++; });
    this->manager1->broadcast(
        createLocalStreamMessage(R"({"hello":"WORLD"})", streamPartId2));
    this->manager2->broadcast(
        createLocalStreamMessage(R"({"hello":"WORLD"})", this->streamPartId));
    blockingWait(waitForCondition(
        [&received1, &received2]() { return received1 >= 1 && received2 >= 1; },
        untilTimeout,
        pollInterval));
}

TEST_F(ContentDeliveryManagerTest, LeavingStreamParts) {
    this->joinBothAndAwaitNeighbors();
    blockingWait(this->manager2->leaveStreamPart(this->streamPartId));
    blockingWait(waitForCondition(
        [this]() {
            return this->manager1->getNeighbors(this->streamPartId).empty();
        },
        untilTimeout,
        pollInterval));
}

TEST_F(ContentDeliveryManagerTest, RttsAreUpdatedForNodeInfo) {
    this->joinBothAndAwaitNeighbors();
    // Wait for the neighbor-update round to record RTTs.
    blockingWait(waitForCondition(
        [this]() {
            const auto info1 = this->manager1->getNodeInfo();
            const auto info2 = this->manager2->getNodeInfo();
            return !info1.empty() &&
                !info1[0].contentDeliveryLayerNeighbors.empty() &&
                info1[0].contentDeliveryLayerNeighbors[0].rtt.has_value() &&
                !info2.empty() &&
                !info2[0].contentDeliveryLayerNeighbors.empty() &&
                info2[0].contentDeliveryLayerNeighbors[0].rtt.has_value();
        },
        untilTimeout,
        pollInterval));
    const auto nodeInfo1 = this->manager1->getNodeInfo();
    EXPECT_GE(nodeInfo1[0].contentDeliveryLayerNeighbors[0].rtt.value(), 0);
}
