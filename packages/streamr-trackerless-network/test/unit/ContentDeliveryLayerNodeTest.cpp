// Ported from packages/trackerless-network/test/unit/
// ContentDeliveryLayerNode.test.ts (v103.8.0-rc.3): view updates driven
// by discovery-layer events, neighbor info, and the
// suppressOwnMessageLoopback delivery flag.
//
// Adaptations: the TS test injects MockHandshaker /
// MockNeighborUpdateManager "as any"; here the factory builds the real
// components, which stay inert within the test's lifetime (the update
// interval is 10 s and no handshakes are triggered). The getInfos
// assertions match by descriptor rather than index because the C++
// NodeList is id-ordered, not insertion-ordered.
//
// NB: NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include) — see TestUtilsTest.cpp for the clangd rationale.
#include <chrono>
#include <memory>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryLayerNode;
import streamr.trackerlessnetwork.createContentDeliveryLayerNode;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.trackerlessnetwork.MockDiscoveryLayerNode;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.TestUtils;
import streamr.trackerlessnetwork.protos;
import streamr.dht.FakeTransport;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.utils.EthereumAddress;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::transport::FakeTransport;
using streamr::trackerlessnetwork::ContentDeliveryLayerNode;
using streamr::trackerlessnetwork::ContentDeliveryLayerNodeOptions;
using streamr::trackerlessnetwork::createContentDeliveryLayerNode;
using streamr::trackerlessnetwork::NodeList;
using streamr::trackerlessnetwork::contentdeliverylayernodeevents::Message;
using streamr::trackerlessnetwork::testutils::
    createMockContentDeliveryRpcRemote;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::trackerlessnetwork::testutils::createStreamMessage;
using streamr::trackerlessnetwork::testutils::MockConnectionLocker;
using streamr::trackerlessnetwork::testutils::MockDiscoveryLayerNode;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::toEthereumAddress;
using streamr::utils::waitForCondition;

namespace dle =
    streamr::trackerlessnetwork::discoverylayer::discoverylayernodeevents;

namespace {
constexpr size_t viewLimit = 10;
constexpr std::chrono::seconds untilTimeout{10};
constexpr std::chrono::milliseconds untilPollInterval{100};
} // namespace

class ContentDeliveryLayerNodeTest : public ::testing::Test {
protected:
    PeerDescriptor peerDescriptor = createMockPeerDescriptor();
    FakeTransport transport{peerDescriptor, [](const auto& /*message*/) {}};
    MockConnectionLocker connectionLocker;
    std::shared_ptr<NodeList> neighbors;
    std::shared_ptr<NodeList> nearbyNodeView;
    std::shared_ptr<NodeList> randomNodeView;
    std::shared_ptr<MockDiscoveryLayerNode> discoveryLayerNode;
    std::shared_ptr<ContentDeliveryLayerNode> node;

    void SetUp() override {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor);
        this->neighbors = std::make_shared<NodeList>(nodeId, viewLimit);
        this->randomNodeView = std::make_shared<NodeList>(nodeId, viewLimit);
        this->nearbyNodeView = std::make_shared<NodeList>(nodeId, viewLimit);
        this->discoveryLayerNode = std::make_shared<MockDiscoveryLayerNode>();
        this->node = this->buildNode(false);
        blockingWait(this->node->start());
    }

    void TearDown() override { this->node->stop(); }

    std::shared_ptr<ContentDeliveryLayerNode> buildNode(
        bool suppressOwnMessageLoopback) {
        return createContentDeliveryLayerNode(
            ContentDeliveryLayerNodeOptions{
                .streamPartId = StreamPartIDUtils::parse("stream#0"),
                .discoveryLayerNode = this->discoveryLayerNode,
                .transport = &this->transport,
                .connectionLocker = &this->connectionLocker,
                .localPeerDescriptor = this->peerDescriptor,
                .isLocalNodeEntryPoint = []() { return false; },
                .neighbors = this->neighbors,
                .nearbyNodeView = this->nearbyNodeView,
                .randomNodeView = this->randomNodeView,
                .suppressOwnMessageLoopback = suppressOwnMessageLoopback});
    }

    // Self-contained node for the suppressOwnMessageLoopback tests (the
    // TS test builds fresh views/transport per node too).
    struct StandaloneNode {
        PeerDescriptor peerDescriptor = createMockPeerDescriptor();
        FakeTransport transport{peerDescriptor, [](const auto& /*message*/) {}};
        MockConnectionLocker connectionLocker;
        std::shared_ptr<MockDiscoveryLayerNode> discoveryLayerNode =
            std::make_shared<MockDiscoveryLayerNode>();
        std::shared_ptr<ContentDeliveryLayerNode> node;

        explicit StandaloneNode(bool suppressOwnMessageLoopback) {
            this->node = createContentDeliveryLayerNode(
                ContentDeliveryLayerNodeOptions{
                    .streamPartId = StreamPartIDUtils::parse("stream#0"),
                    .discoveryLayerNode = this->discoveryLayerNode,
                    .transport = &this->transport,
                    .connectionLocker = &this->connectionLocker,
                    .localPeerDescriptor = this->peerDescriptor,
                    .isLocalNodeEntryPoint = []() { return false; },
                    .suppressOwnMessageLoopback = suppressOwnMessageLoopback});
        }

        ~StandaloneNode() { this->node->stop(); }
    };

    static StreamMessage ownMessage() {
        return createStreamMessage(
            "x",
            StreamPartIDUtils::parse("stream#0"),
            toEthereumAddress("0x1234567890123456789012345678901234567890"));
    }
};

TEST_F(ContentDeliveryLayerNodeTest, GetNeighbors) {
    const auto mockRemote = createMockContentDeliveryRpcRemote();
    this->neighbors->add(mockRemote);
    const auto result = this->node->getNeighbors();
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(
        Identifiers::getNodeIdFromPeerDescriptor(result[0]),
        Identifiers::getNodeIdFromPeerDescriptor(
            mockRemote->getPeerDescriptor()));
}

TEST_F(ContentDeliveryLayerNodeTest, GetNearbyNodeView) {
    const auto mockRemote = createMockContentDeliveryRpcRemote();
    this->nearbyNodeView->add(mockRemote);
    const auto ids = this->node->getNearbyNodeView().getIds();
    ASSERT_EQ(ids.size(), 1);
    EXPECT_EQ(
        ids[0],
        Identifiers::getNodeIdFromPeerDescriptor(
            mockRemote->getPeerDescriptor()));
}

TEST_F(ContentDeliveryLayerNodeTest, AddsClosestNodesToNearbyNodeView) {
    const auto peerDescriptor1 = createMockPeerDescriptor();
    const auto peerDescriptor2 = createMockPeerDescriptor();
    this->discoveryLayerNode->setClosestContacts(
        {peerDescriptor1, peerDescriptor2});
    this->discoveryLayerNode->emit<dle::NearbyContactAdded>(peerDescriptor1);
    blockingWait(waitForCondition(
        [this]() { return this->nearbyNodeView->size() == 2; },
        untilTimeout,
        untilPollInterval));
    EXPECT_TRUE(
        this->nearbyNodeView
            ->get(Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor1))
            .has_value());
    EXPECT_TRUE(
        this->nearbyNodeView
            ->get(Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor2))
            .has_value());
}

TEST_F(ContentDeliveryLayerNodeTest, AddsRandomNodesToRandomNodeView) {
    const auto peerDescriptor1 = createMockPeerDescriptor();
    const auto peerDescriptor2 = createMockPeerDescriptor();
    this->discoveryLayerNode->setRandomContacts(
        {peerDescriptor1, peerDescriptor2});
    this->discoveryLayerNode->emit<dle::RandomContactAdded>(peerDescriptor1);
    blockingWait(waitForCondition(
        [this]() { return this->randomNodeView->size() == 2; },
        untilTimeout,
        untilPollInterval));
    EXPECT_TRUE(
        this->randomNodeView
            ->get(Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor1))
            .has_value());
    EXPECT_TRUE(
        this->randomNodeView
            ->get(Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor2))
            .has_value());
}

TEST_F(ContentDeliveryLayerNodeTest, AddsDiscoveryNeighborsToNearbyNodeView) {
    const auto peerDescriptor1 = createMockPeerDescriptor();
    const auto peerDescriptor2 = createMockPeerDescriptor();
    this->discoveryLayerNode->addNewRandomPeerToKBucket();
    this->discoveryLayerNode->setClosestContacts(
        {peerDescriptor1, peerDescriptor2});
    this->discoveryLayerNode->emit<dle::NearbyContactAdded>(peerDescriptor1);
    blockingWait(waitForCondition(
        [this]() { return this->nearbyNodeView->size() == 3; },
        untilTimeout,
        untilPollInterval));
    EXPECT_TRUE(
        this->nearbyNodeView
            ->get(Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor1))
            .has_value());
    EXPECT_TRUE(
        this->nearbyNodeView
            ->get(Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor2))
            .has_value());
}

TEST_F(ContentDeliveryLayerNodeTest, GetInfos) {
    const auto nodeWithRtt = createMockContentDeliveryRpcRemote();
    this->neighbors->add(nodeWithRtt);
    const auto nodeWithoutRtt = createMockContentDeliveryRpcRemote();
    this->neighbors->add(nodeWithoutRtt);
    nodeWithRtt->setRtt(100); // NOLINT
    const auto infos = this->node->getInfos();
    ASSERT_EQ(infos.size(), 2);
    const auto rttNodeId = Identifiers::getNodeIdFromPeerDescriptor(
        nodeWithRtt->getPeerDescriptor());
    for (const auto& info : infos) {
        if (Identifiers::getNodeIdFromPeerDescriptor(info.peerDescriptor) ==
            rttNodeId) {
            EXPECT_EQ(info.rtt, 100);
        } else {
            EXPECT_EQ(info.rtt.has_value(), false);
        }
    }
}

TEST_F(ContentDeliveryLayerNodeTest, OwnPublishDeliveredByDefault) {
    // The fixture node has suppressOwnMessageLoopback = false.
    size_t received = 0;
    this->node->on<Message>(
        [&received](const StreamMessage& /*msg*/) { received++; });
    this->node->broadcast(this->ownMessage()); // no previousNode
    EXPECT_EQ(received, 1);
}

TEST_F(ContentDeliveryLayerNodeTest, SuppressedOwnPublishNotDelivered) {
    StandaloneNode standalone{true};
    blockingWait(standalone.node->start());
    size_t received = 0;
    standalone.node->on<Message>(
        [&received](const StreamMessage& /*msg*/) { received++; });
    standalone.node->broadcast(this->ownMessage()); // no previousNode
    EXPECT_EQ(received, 0);
}

TEST_F(ContentDeliveryLayerNodeTest, SuppressedForwardedMessageIsDelivered) {
    StandaloneNode standalone{true};
    blockingWait(standalone.node->start());
    size_t received = 0;
    standalone.node->on<Message>(
        [&received](const StreamMessage& /*msg*/) { received++; });
    standalone.node->broadcast(
        this->ownMessage(), Identifiers::createRandomDhtAddress());
    EXPECT_EQ(received, 1);
}
