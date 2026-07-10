// Ported from packages/dht/test/integration/Mock-Layer1-Layer0.test.ts
// (v103.8.0-rc.3): five layer-0 DhtNodes over the Simulator, each carrying a
// layer-1 DhtNode that uses the layer-0 node as its transport. After all
// nodes join both layers, each layer-1 node must converge on the same
// neighbour set as its layer-0 node (the layers share the physical
// connections, so the topologies coincide in this small network).
#include <memory>
#include <set>
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
import streamr.dht.protos;

#include "DhtNodeTestUtils.hpp" // IWYU pragma: keep — needs the imports above

using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::testutils::createMockConnectionDhtNode;
using streamr::dht::testutils::createMockConnectionLayer1Node;
using streamr::dht::testutils::MockDhtNode;
using streamr::utils::blockingWait;

namespace {

std::set<std::string> toNodeIdSet(
    const std::vector<PeerDescriptor>& descriptors) {
    std::set<std::string> ids;
    for (const auto& descriptor : descriptors) {
        ids.insert(
            static_cast<std::string>(
                Identifiers::getNodeIdFromPeerDescriptor(descriptor)));
    }
    return ids;
}

} // namespace

class MockLayer1Layer0Test : public ::testing::Test {
protected:
    Simulator simulator;
    std::shared_ptr<MockDhtNode> layer0EntryPoint;
    std::shared_ptr<MockDhtNode> layer0Node1;
    std::shared_ptr<MockDhtNode> layer0Node2;
    std::shared_ptr<MockDhtNode> layer0Node3;
    std::shared_ptr<MockDhtNode> layer0Node4;
    std::shared_ptr<MockDhtNode> layer1EntryPoint;
    std::shared_ptr<MockDhtNode> layer1Node1;
    std::shared_ptr<MockDhtNode> layer1Node2;
    std::shared_ptr<MockDhtNode> layer1Node3;
    std::shared_ptr<MockDhtNode> layer1Node4;

    void SetUp() override {
        this->layer0EntryPoint = createMockConnectionDhtNode(
            this->simulator, Identifiers::createRandomDhtAddress());
        this->layer0Node1 = createMockConnectionDhtNode(
            this->simulator, Identifiers::createRandomDhtAddress());
        this->layer0Node2 = createMockConnectionDhtNode(
            this->simulator, Identifiers::createRandomDhtAddress());
        this->layer0Node3 = createMockConnectionDhtNode(
            this->simulator, Identifiers::createRandomDhtAddress());
        this->layer0Node4 = createMockConnectionDhtNode(
            this->simulator, Identifiers::createRandomDhtAddress());

        this->layer1EntryPoint =
            createMockConnectionLayer1Node(*this->layer0EntryPoint->node);
        this->layer1Node1 =
            createMockConnectionLayer1Node(*this->layer0Node1->node);
        this->layer1Node2 =
            createMockConnectionLayer1Node(*this->layer0Node2->node);
        this->layer1Node3 =
            createMockConnectionLayer1Node(*this->layer0Node3->node);
        this->layer1Node4 =
            createMockConnectionLayer1Node(*this->layer0Node4->node);

        const auto entryPointDescriptor =
            this->layer0EntryPoint->node->getLocalPeerDescriptor();
        blockingWait(
            this->layer0EntryPoint->node->joinDht({entryPointDescriptor}));
        blockingWait(
            this->layer1EntryPoint->node->joinDht({entryPointDescriptor}));
    }

    void TearDown() override {
        // Layer-1 nodes first: they use the layer-0 nodes as transports.
        this->layer1EntryPoint->stopNode();
        this->layer1Node1->stopNode();
        this->layer1Node2->stopNode();
        this->layer1Node3->stopNode();
        this->layer1Node4->stopNode();
        this->layer0EntryPoint->stopNode();
        this->layer0Node1->stopNode();
        this->layer0Node2->stopNode();
        this->layer0Node3->stopNode();
        this->layer0Node4->stopNode();
        this->simulator.stop();
    }
};

TEST_F(MockLayer1Layer0Test, HappyPath) {
    const auto entryPointDescriptor =
        this->layer0EntryPoint->node->getLocalPeerDescriptor();
    blockingWait(this->layer0Node1->node->joinDht({entryPointDescriptor}));
    blockingWait(this->layer0Node2->node->joinDht({entryPointDescriptor}));
    blockingWait(this->layer0Node3->node->joinDht({entryPointDescriptor}));
    blockingWait(this->layer0Node4->node->joinDht({entryPointDescriptor}));

    blockingWait(this->layer1Node1->node->joinDht({entryPointDescriptor}));
    blockingWait(this->layer1Node2->node->joinDht({entryPointDescriptor}));
    blockingWait(this->layer1Node3->node->joinDht({entryPointDescriptor}));
    blockingWait(this->layer1Node4->node->joinDht({entryPointDescriptor}));

    EXPECT_EQ(
        this->layer1Node1->node->getNeighborCount(),
        this->layer0Node1->node->getNeighborCount());
    EXPECT_EQ(
        this->layer1Node2->node->getNeighborCount(),
        this->layer0Node2->node->getNeighborCount());
    EXPECT_EQ(
        this->layer1Node3->node->getNeighborCount(),
        this->layer0Node3->node->getNeighborCount());
    EXPECT_EQ(
        this->layer1Node4->node->getNeighborCount(),
        this->layer0Node4->node->getNeighborCount());

    EXPECT_EQ(
        toNodeIdSet(this->layer1Node1->node->getNeighbors()),
        toNodeIdSet(this->layer0Node1->node->getNeighbors()));
    EXPECT_EQ(
        toNodeIdSet(this->layer1Node2->node->getNeighbors()),
        toNodeIdSet(this->layer0Node2->node->getNeighbors()));
    EXPECT_EQ(
        toNodeIdSet(this->layer1Node3->node->getNeighbors()),
        toNodeIdSet(this->layer0Node3->node->getNeighbors()));
    EXPECT_EQ(
        toNodeIdSet(this->layer1Node4->node->getNeighbors()),
        toNodeIdSet(this->layer0Node4->node->getNeighbors()));
}
