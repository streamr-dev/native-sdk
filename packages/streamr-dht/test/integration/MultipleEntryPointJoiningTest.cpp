// Ported from packages/dht/test/integration/MultipleEntryPointJoining.test.ts
// (v103.8.0-rc.3): DhtNodes over the Simulator joining through several entry
// points at once (deferred here from phase A7). Exercises the whole node —
// PeerDiscovery, Router, RecursiveOperationManager and the RPC stack — over
// real simulated connections.
#include <chrono>
#include <cstddef>
#include <memory>
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
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::testutils::createMockConnectionDhtNode;
using streamr::dht::testutils::MockDhtNode;
using streamr::utils::blockingWait;

class MultipleEntryPointJoiningTest : public ::testing::Test {
protected:
    Simulator simulator{LatencyType::REAL};
    std::shared_ptr<MockDhtNode> node1;
    std::shared_ptr<MockDhtNode> node2;
    std::shared_ptr<MockDhtNode> node3;
    std::vector<PeerDescriptor> entryPoints;

    void SetUp() override {
        this->node1 = createMockConnectionDhtNode(this->simulator);
        this->node2 = createMockConnectionDhtNode(this->simulator);
        this->node3 = createMockConnectionDhtNode(this->simulator);
        this->entryPoints = {
            this->node1->node->getLocalPeerDescriptor(),
            this->node2->node->getLocalPeerDescriptor(),
            this->node3->node->getLocalPeerDescriptor()};
    }

    void TearDown() override {
        this->node1->stopNode();
        this->node2->stopNode();
        this->node3->stopNode();
        this->simulator.stop();
    }
};

TEST_F(MultipleEntryPointJoiningTest, CanJoinSimultaneously) {
    blockingWait(
        folly::coro::collectAll(
            this->node1->node->joinDht(this->entryPoints),
            this->node2->node->joinDht(this->entryPoints),
            this->node3->node->joinDht(this->entryPoints)));
    EXPECT_EQ(this->node1->node->getNeighborCount(), 2U);
    EXPECT_EQ(this->node2->node->getNeighborCount(), 2U);
    EXPECT_EQ(this->node3->node->getNeighborCount(), 2U);
}

TEST_F(MultipleEntryPointJoiningTest, CanJoinEvenIfANodeIsOffline) {
    this->node3->stopNode();
    blockingWait(
        folly::coro::collectAll(
            this->node1->node->joinDht(this->entryPoints),
            this->node2->node->joinDht(this->entryPoints)));
    EXPECT_EQ(this->node1->node->getNeighborCount(), 1U);
    EXPECT_EQ(this->node2->node->getNeighborCount(), 1U);
}

class JoinViaMultipleEntryPointsTest : public ::testing::Test {
protected:
    Simulator simulator{LatencyType::REAL};
    std::shared_ptr<MockDhtNode> entryPoint1;
    std::shared_ptr<MockDhtNode> entryPoint2;
    std::shared_ptr<MockDhtNode> node1;
    std::shared_ptr<MockDhtNode> node2;
    std::vector<PeerDescriptor> entryPoints;

    void SetUp() override {
        this->entryPoint1 = createMockConnectionDhtNode(this->simulator);
        this->entryPoint2 = createMockConnectionDhtNode(this->simulator);
        this->node1 = createMockConnectionDhtNode(this->simulator);
        this->node2 = createMockConnectionDhtNode(this->simulator);
        this->entryPoints = {
            this->entryPoint1->node->getLocalPeerDescriptor(),
            this->entryPoint2->node->getLocalPeerDescriptor()};
        blockingWait(this->entryPoint1->node->joinDht(this->entryPoints));
        blockingWait(this->entryPoint2->node->joinDht(this->entryPoints));
    }

    void TearDown() override {
        this->entryPoint1->stopNode();
        this->entryPoint2->stopNode();
        this->node1->stopNode();
        this->node2->stopNode();
        this->simulator.stop();
    }
};

// Regression test for the phase-AA concurrent-connect defects (see
// trackerless-network-completion-plan.md, "Phase AA"): the joining node's
// k-bucket ping and its discovery query race to connect the same new peer on
// different threads. Fixed by (1) send() no longer closing the connector-shared
// pending connection on a tie-break rejection and (2) ConnectingEndpointState
// preserving message boundaries when flushing its buffer.
TEST_F(JoinViaMultipleEntryPointsTest, NonEntryPointNodesCanJoin) {
    blockingWait(this->node1->node->joinDht(this->entryPoints));
    EXPECT_EQ(this->node1->node->getNeighborCount(), 2U);
    blockingWait(this->node2->node->joinDht(this->entryPoints));
    EXPECT_EQ(this->node2->node->getNeighborCount(), 3U);
}
