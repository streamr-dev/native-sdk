// Ported from packages/trackerless-network/test/integration/
// ContentDeliveryLayerNode-Layer1Node.test.ts and its -Latencies
// variant (v103.8.0-rc.3): content delivery nodes stacked on real
// layer-1 DhtNodes over simulator transports — single node, 4 nodes
// with bidirectionality, and the 64-node scale case; the Latencies
// fixture runs the same protocol over a 50 ms fixed-latency simulator
// (the TS files differ only in that and in timeouts, so the test
// bodies are shared).
//
// NB: NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include) — see TestUtilsTest.cpp for the clangd rationale.
#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryLayerNode;
import streamr.trackerlessnetwork.createContentDeliveryLayerNode;
import streamr.trackerlessnetwork.DhtNodeDiscoveryLayer;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.protos;
import streamr.logger.SLogger;
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
using streamr::logger::SLogger;
using streamr::trackerlessnetwork::ContentDeliveryLayerNode;
using streamr::trackerlessnetwork::ContentDeliveryLayerNodeOptions;
using streamr::trackerlessnetwork::createContentDeliveryLayerNode;
using streamr::trackerlessnetwork::discoverylayer::DhtNodeDiscoveryLayer;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::waitForCondition;

namespace {

// Local copy of the TestUtils factory: importing the TestUtils module on
// top of this TU's DhtNode + simulator + content-delivery composition
// exhausts clang's per-TU source-location space.
inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

constexpr size_t otherNodeCount = 64;
constexpr std::chrono::milliseconds neighborUpdateInterval{2000};
constexpr std::chrono::seconds untilTimeout{15};
constexpr std::chrono::seconds scaleTimeout{30};
constexpr std::chrono::milliseconds pollInterval{100};
constexpr size_t neighborTarget = 4;
constexpr double fixedLatencyMs = 50.0;

// The TS createMockContentDeliveryLayerNodeAndDhtNode helper: one
// simulator transport + a layer-1 DhtNode wrapped as the discovery
// layer + a content-delivery node on top.
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
            .isLocalNodeEntryPoint = []() { return false; },
            .neighborUpdateInterval = neighborUpdateInterval});
    return SimNode{
        .transport = std::move(transport),
        .discoveryLayerNode = std::move(discoveryLayerNode),
        .contentDeliveryLayerNode = std::move(contentDeliveryLayerNode)};
}

} // namespace

class ContentDeliveryLayerNodeLayer1Test : public ::testing::Test {
protected:
    StreamPartID streamPartId = StreamPartIDUtils::parse("stream#0");
    PeerDescriptor entrypointDescriptor = createMockPeerDescriptor();
    Simulator simulator;
    SimNode entryPoint;
    std::vector<SimNode> others;

    ContentDeliveryLayerNodeLayer1Test() : simulator(LatencyType::NONE) {}
    ContentDeliveryLayerNodeLayer1Test(
        LatencyType latencyType, std::optional<double> fixedLatencyMs)
        : simulator(latencyType, fixedLatencyMs) {}

    void SetUp() override {
        this->entryPoint = createSimNode(
            this->entrypointDescriptor, this->streamPartId, this->simulator);
        this->others.reserve(otherNodeCount);
        for (size_t i = 0; i < otherNodeCount; i++) {
            this->others.push_back(createSimNode(
                createMockPeerDescriptor(),
                this->streamPartId,
                this->simulator));
        }
        blockingWait(this->entryPoint.discoveryLayerNode->start());
        blockingWait(this->entryPoint.contentDeliveryLayerNode->start());
        blockingWait(this->entryPoint.discoveryLayerNode->joinDht(
            {this->entrypointDescriptor}));
        {
            std::vector<folly::coro::Task<void>> starts;
            starts.reserve(otherNodeCount);
            for (auto& other : this->others) {
                starts.push_back(other.discoveryLayerNode->start());
            }
            blockingWait(folly::coro::collectAllRange(std::move(starts)));
        }
    }

    void TearDown() override {
        blockingWait(this->entryPoint.discoveryLayerNode->stop());
        this->entryPoint.contentDeliveryLayerNode->stop();
        for (auto& other : this->others) {
            blockingWait(other.discoveryLayerNode->stop());
            other.contentDeliveryLayerNode->stop();
        }
        this->entryPoint.transport->stop();
        for (auto& other : this->others) {
            other.transport->stop();
        }
        this->simulator.stop();
    }

    void runHappyPathSingleNode() {
        blockingWait(this->others[0].contentDeliveryLayerNode->start());
        blockingWait(this->others[0].discoveryLayerNode->joinDht(
            {this->entrypointDescriptor}));

        auto& node = *this->others[0].contentDeliveryLayerNode;
        blockingWait(waitForCondition(
            [&node]() { return node.getNeighbors().size() == 1; },
            untilTimeout,
            pollInterval));
        EXPECT_EQ(node.getNearbyNodeView().getIds().size(), 1);
        EXPECT_EQ(node.getNeighbors().size(), 1);
    }

    void runHappyPathFourNodes() {
        constexpr size_t nodeCount = 4;
        for (size_t i = 0; i < nodeCount; i++) {
            blockingWait(this->others[i].contentDeliveryLayerNode->start());
        }
        {
            std::vector<folly::coro::Task<void>> joins;
            joins.reserve(nodeCount);
            for (size_t i = 0; i < nodeCount; i++) {
                joins.push_back(this->others[i].discoveryLayerNode->joinDht(
                    {this->entrypointDescriptor}));
            }
            blockingWait(folly::coro::collectAllRange(std::move(joins)));
        }
        blockingWait(waitForCondition(
            [this]() {
                for (size_t i = 0; i < nodeCount; i++) {
                    if (this->others[i]
                            .contentDeliveryLayerNode->getNeighbors()
                            .size() != neighborTarget) {
                        return false;
                    }
                }
                return true;
            },
            untilTimeout,
            pollInterval));
        for (size_t i = 0; i < nodeCount; i++) {
            EXPECT_GE(
                this->others[i]
                    .contentDeliveryLayerNode->getNearbyNodeView()
                    .getIds()
                    .size(),
                neighborTarget);
            EXPECT_GE(
                this->others[i].contentDeliveryLayerNode->getNeighbors().size(),
                neighborTarget);
        }

        // Check bidirectionality across the four nodes + the entry point.
        std::vector<ContentDeliveryLayerNode*> allNodes;
        allNodes.reserve(nodeCount + 1);
        for (size_t i = 0; i < nodeCount; i++) {
            allNodes.push_back(this->others[i].contentDeliveryLayerNode.get());
        }
        allNodes.push_back(this->entryPoint.contentDeliveryLayerNode.get());
        for (auto* node : allNodes) {
            for (const auto& nodeId : node->getNearbyNodeView().getIds()) {
                const auto neighborIt = std::ranges::find_if(
                    allNodes, [&nodeId](ContentDeliveryLayerNode* candidate) {
                        return candidate->getOwnNodeId() == nodeId;
                    });
                ASSERT_NE(neighborIt, allNodes.end());
                const auto neighborsOfNeighbor = (*neighborIt)->getNeighbors();
                const auto includesNode = std::ranges::any_of(
                    neighborsOfNeighbor,
                    [node](const PeerDescriptor& descriptor) {
                        return Identifiers::getNodeIdFromPeerDescriptor(
                                   descriptor) == node->getOwnNodeId();
                    });
                EXPECT_EQ(includesNode, true);
            }
        }
    }

    void runHappyPathSixtyFourNodes() {
        for (auto& other : this->others) {
            blockingWait(other.contentDeliveryLayerNode->start());
        }
        {
            std::vector<folly::coro::Task<void>> joins;
            joins.reserve(otherNodeCount);
            for (auto& other : this->others) {
                joins.push_back(other.discoveryLayerNode->joinDht(
                    {this->entrypointDescriptor}));
            }
            blockingWait(folly::coro::collectAllRange(std::move(joins)));
        }
        blockingWait(waitForCondition(
            [this]() {
                return std::ranges::all_of(this->others, [](const auto& other) {
                    return other.contentDeliveryLayerNode->getNeighbors()
                               .size() >= neighborTarget;
                });
            },
            scaleTimeout,
            pollInterval));

        size_t neighborSum = 0;
        for (const auto& other : this->others) {
            neighborSum +=
                other.contentDeliveryLayerNode->getNeighbors().size();
        }
        SLogger::info(
            "AVG number of neighbors: " +
            std::to_string(static_cast<double>(neighborSum) / otherNodeCount));

        blockingWait(waitForCondition(
            [this]() {
                return std::ranges::all_of(this->others, [](const auto& other) {
                    return other.contentDeliveryLayerNode
                               ->getOutgoingHandshakeCount() == 0;
                });
            },
            scaleTimeout,
            pollInterval));

        // TS NET-1074: sometimes unidirectional connections remain — accept
        // up to 2 mismatches.
        blockingWait(waitForCondition(
            [this]() {
                size_t mismatchCounter = 0;
                for (const auto& other : this->others) {
                    const auto nodeId =
                        other.contentDeliveryLayerNode->getOwnNodeId();
                    for (const auto& neighbor :
                         other.contentDeliveryLayerNode->getNeighbors()) {
                        const auto neighborId =
                            Identifiers::getNodeIdFromPeerDescriptor(neighbor);
                        if (neighborId ==
                            this->entryPoint.contentDeliveryLayerNode
                                ->getOwnNodeId()) {
                            continue;
                        }
                        const auto neighborIt = std::ranges::find_if(
                            this->others, [&neighborId](const auto& candidate) {
                                return candidate.contentDeliveryLayerNode
                                           ->getOwnNodeId() == neighborId;
                            });
                        if (neighborIt == this->others.end()) {
                            continue;
                        }
                        const auto neighborsOfNeighbor =
                            neighborIt->contentDeliveryLayerNode
                                ->getNeighbors();
                        const auto includesNode = std::ranges::any_of(
                            neighborsOfNeighbor,
                            [&nodeId](const PeerDescriptor& descriptor) {
                                return Identifiers::getNodeIdFromPeerDescriptor(
                                           descriptor) == nodeId;
                            });
                        if (!includesNode) {
                            mismatchCounter++;
                        }
                    }
                }
                return mismatchCounter <= 2;
            },
            scaleTimeout,
            std::chrono::seconds(1)));
    }
};

// The -Latencies TS variant: same protocol over 50 ms fixed latency.
class ContentDeliveryLayerNodeLayer1LatenciesTest
    : public ContentDeliveryLayerNodeLayer1Test {
protected:
    ContentDeliveryLayerNodeLayer1LatenciesTest()
        : ContentDeliveryLayerNodeLayer1Test(
              LatencyType::FIXED, fixedLatencyMs) {}
};

TEST_F(ContentDeliveryLayerNodeLayer1Test, HappyPathSingleNode) {
    this->runHappyPathSingleNode();
}

TEST_F(ContentDeliveryLayerNodeLayer1Test, HappyPathFourNodes) {
    this->runHappyPathFourNodes();
}

TEST_F(ContentDeliveryLayerNodeLayer1Test, HappyPathSixtyFourNodes) {
    this->runHappyPathSixtyFourNodes();
}

TEST_F(ContentDeliveryLayerNodeLayer1LatenciesTest, HappyPathSingleNode) {
    this->runHappyPathSingleNode();
}

TEST_F(ContentDeliveryLayerNodeLayer1LatenciesTest, HappyPathFourNodes) {
    this->runHappyPathFourNodes();
}

TEST_F(ContentDeliveryLayerNodeLayer1LatenciesTest, HappyPathSixtyFourNodes) {
    this->runHappyPathSixtyFourNodes();
}
