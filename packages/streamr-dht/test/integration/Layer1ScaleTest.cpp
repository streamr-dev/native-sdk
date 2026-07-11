// Ported from packages/dht/test/integration/Layer1-scale.test.ts
// (v103.8.0-rc.3): 48 layer-0 DhtNodes over the Simulator join in parallel;
// then layer-1 DhtNodes stacked on them (one service, and four services in
// the second test) join in parallel, and every layer-1 node must see exactly
// its layer-0 node's connections (the layers share one physical connection
// set). The commented-out TS 'layer1 routing' case is not ported (disabled
// upstream: "TODO: Make this work").
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.RegionPings;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.TestUtils;
import streamr.dht.protos;

#include "DhtNodeTestUtils.hpp" // IWYU pragma: keep — needs the imports above

using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::numberOfNodesPerKBucketDefault;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::testutils::createMockConnectionDhtNode;
using streamr::dht::testutils::createMockConnectionLayer1Node;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::testutils::MockDhtNode;
using streamr::utils::blockingWait;

namespace {

constexpr size_t nodeCount = 48;

// Promise.all(nodes.map((node) => node.joinDht([entryPoint])))
void joinAllInParallel(
    const std::vector<std::shared_ptr<MockDhtNode>>& nodes,
    const PeerDescriptor& entryPointDescriptor) {
    std::vector<folly::coro::Task<void>> joins;
    joins.reserve(nodes.size());
    for (const auto& node : nodes) {
        joins.push_back(node->node->joinDht({entryPointDescriptor}));
    }
    blockingWait(folly::coro::collectAllRange(std::move(joins)));
}

} // namespace

class Layer1ScaleTest : public ::testing::Test {
protected:
    Simulator simulator;
    PeerDescriptor entryPoint0Descriptor = createMockPeerDescriptor();
    std::shared_ptr<MockDhtNode> layer0EntryPoint;
    std::vector<std::shared_ptr<MockDhtNode>> nodes;
    std::vector<std::shared_ptr<MockDhtNode>> layer1CleanUp;

    void SetUp() override {
        this->layer0EntryPoint = createMockConnectionDhtNode(
            this->simulator,
            Identifiers::getNodeIdFromPeerDescriptor(
                this->entryPoint0Descriptor));
        blockingWait(this->layer0EntryPoint->node->joinDht(
            {this->entryPoint0Descriptor}));

        for (size_t i = 0; i < nodeCount; i++) {
            this->nodes.push_back(createMockConnectionDhtNode(this->simulator));
        }
        joinAllInParallel(this->nodes, this->entryPoint0Descriptor);
    }

    void TearDown() override {
        for (const auto& node : this->layer1CleanUp) {
            node->stopNode();
        }
        for (const auto& node : this->nodes) {
            node->stopNode();
        }
        this->layer0EntryPoint->stopNode();
        this->simulator.stop();
    }
};

TEST_F(Layer1ScaleTest, SingleLayer1Dht) {
    auto layer1EntryPoint =
        createMockConnectionLayer1Node(*this->layer0EntryPoint->node);
    blockingWait(
        layer1EntryPoint->node->joinDht({this->entryPoint0Descriptor}));
    this->layer1CleanUp.push_back(layer1EntryPoint);

    std::vector<std::shared_ptr<MockDhtNode>> layer1Nodes;
    for (size_t i = 0; i < nodeCount; i++) {
        auto layer1 = createMockConnectionLayer1Node(*this->nodes[i]->node);
        layer1Nodes.push_back(layer1);
        this->layer1CleanUp.push_back(layer1);
    }

    joinAllInParallel(layer1Nodes, this->entryPoint0Descriptor);

    for (size_t i = 0; i < nodeCount; i++) {
        const auto& layer0Node = this->nodes[i]->node;
        const auto& layer1Node = layer1Nodes[i]->node;
        EXPECT_EQ(layer1Node->getNodeId(), layer0Node->getNodeId());
        // TS compares the two views' connection counts and connection
        // lists; both are reads of the SAME live view object (the layer-1
        // node is constructed with the layer-0 node's ConnectionsView), so
        // in single-threaded TS the equalities hold trivially. In C++ the
        // background connection churn (e.g. trailing k-bucket pings) can
        // mutate the view between two reads, so assert the identity that
        // makes the TS equalities true instead of racing two snapshots.
        EXPECT_EQ(
            layer1Node->getConnectionsView(), layer0Node->getConnectionsView());
        EXPECT_GE(
            layer1Node->getNeighborCount(), numberOfNodesPerKBucketDefault / 2);
    }
}

TEST_F(Layer1ScaleTest, MultipleLayer1Dht) {
    const std::vector<std::string> serviceIds = {"one", "two", "three", "four"};
    for (const auto& serviceId : serviceIds) {
        auto entryPoint = createMockConnectionLayer1Node(
            *this->layer0EntryPoint->node, serviceId);
        blockingWait(entryPoint->node->joinDht({this->entryPoint0Descriptor}));
        this->layer1CleanUp.push_back(entryPoint);
    }

    std::vector<std::vector<std::shared_ptr<MockDhtNode>>> streams(
        serviceIds.size());
    for (size_t i = 0; i < nodeCount; i++) {
        for (size_t s = 0; s < serviceIds.size(); s++) {
            auto layer1 = createMockConnectionLayer1Node(
                *this->nodes[i]->node, serviceIds[s]);
            streams[s].push_back(layer1);
            this->layer1CleanUp.push_back(layer1);
        }
    }

    // TS joins layer1CleanUp (the already-joined entry points re-join too).
    joinAllInParallel(this->layer1CleanUp, this->entryPoint0Descriptor);

    for (size_t i = 0; i < nodeCount; i++) {
        const auto& layer0Node = this->nodes[i]->node;
        for (size_t s = 0; s < serviceIds.size(); s++) {
            // Same live-view identity as in SingleLayer1Dht: TS compares
            // counts of the same object; two C++ reads would race the
            // background connection churn.
            EXPECT_EQ(
                layer0Node->getConnectionsView(),
                streams[s][i]->node->getConnectionsView());
        }
    }
}
