// Ported from packages/dht/test/benchmark/KademliaCorrectness.test.ts
// (v103.8.0-rc.3), adapted per the completion plan ("as a slow correctness
// test, not a benchmark"):
//  - the TS test reads pre-generated ground-truth files (test/data/*.json,
//    produced by `npm run prepare-kademlia-simulation` and not in the repo);
//    the ground truth is simply each node's neighbours ordered by Kademlia
//    XOR distance, so it is computed in-test with getClosestNodes over the
//    actual node ids instead of loaded from files (which also lets the ids
//    be random per run);
//  - the TS test only prints the statistics; here they are asserted with
//    conservative floors so regressions in join/discovery correctness fail
//    the test.
// Like TS, a node's "correct neighbours" is the length of the matching
// PREFIX between its getClosestContacts(8) ordering and the ground-truth
// ordering.
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.logger.SLogger;
import streamr.utils.CoroutineHelper;
import streamr.dht.DhtNode;
import streamr.dht.getClosestNodes;
import streamr.dht.Identifiers;
import streamr.dht.RegionPings;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.protos;

#include "DhtNodeTestUtils.hpp" // IWYU pragma: keep — needs the imports above

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::contact::GetClosestNodesOptions;
using streamr::dht::testutils::createMockConnectionDhtNode;
using streamr::dht::testutils::MockDhtNode;
using streamr::utils::blockingWait;

namespace {

constexpr size_t numNodes = 200;
constexpr size_t neighborsToCheck = 8;

} // namespace

class KademliaCorrectnessTest : public ::testing::Test {
protected:
    Simulator simulator;
    std::vector<std::shared_ptr<MockDhtNode>> nodes;

    void SetUp() override {
        for (size_t i = 0; i < numNodes; i++) {
            this->nodes.push_back(createMockConnectionDhtNode(this->simulator));
        }
    }

    void TearDown() override {
        for (const auto& node : this->nodes) {
            node->stopNode();
        }
        this->simulator.stop();
    }
};

TEST_F(KademliaCorrectnessTest, CanFindCorrectNeighbors) {
    const auto& entryPoint = this->nodes[0];
    const auto entryPointDescriptor =
        entryPoint->node->getLocalPeerDescriptor();
    blockingWait(entryPoint->node->joinDht({entryPointDescriptor}));
    for (size_t i = 1; i < numNodes; i++) {
        blockingWait(this->nodes[i]->node->joinDht({entryPointDescriptor}));
    }

    // Ground truth: for each node, every other node's descriptor ordered by
    // Kademlia XOR distance from it (what the TS pre-generated data holds).
    std::vector<PeerDescriptor> allDescriptors;
    allDescriptors.reserve(numNodes);
    for (const auto& node : this->nodes) {
        allDescriptors.push_back(node->node->getLocalPeerDescriptor());
    }

    size_t minimumCorrectNeighbors = numNodes;
    size_t sumCorrectNeighbors = 0;
    size_t sumKbucketSize = 1;
    size_t sumKnownOfClosest = 0;

    for (size_t i = 0; i < numNodes; i++) {
        const auto nodeId = this->nodes[i]->node->getNodeId();
        std::vector<PeerDescriptor> others;
        others.reserve(numNodes - 1);
        for (size_t j = 0; j < numNodes; j++) {
            if (j != i) {
                others.push_back(allDescriptors[j]);
            }
        }
        const auto groundTruth = getClosestNodes(
            nodeId,
            others,
            GetClosestNodesOptions{.maxCount = neighborsToCheck});

        const auto kademliaNeighbors =
            this->nodes[i]->node->getClosestContacts(neighborsToCheck);

        size_t correctNeighbors = 0;
        for (size_t j = 0;
             j < groundTruth.size() && j < kademliaNeighbors.size();
             j++) {
            if (!Identifiers::areEqualPeerDescriptors(
                    groundTruth[j], kademliaNeighbors[j])) {
                break;
            }
            correctNeighbors++;
        }
        minimumCorrectNeighbors =
            std::min(minimumCorrectNeighbors, correctNeighbors);
        // Order-insensitive complement of the strict prefix metric above:
        // how many of the true closest-8 the node knows AT ALL (anywhere in
        // its top-8). Separates "does not know its closest peers" (a real
        // discovery failure) from "knows them in slightly different order"
        // (inherent to Kademlia's liveness-biased buckets).
        size_t knownOfClosest = 0;
        for (const auto& truth : groundTruth) {
            for (const auto& neighbor : kademliaNeighbors) {
                if (Identifiers::areEqualPeerDescriptors(truth, neighbor)) {
                    knownOfClosest++;
                    break;
                }
            }
        }
        if (i > 0) {
            sumKbucketSize += this->nodes[i]->node->getNeighborCount();
            sumCorrectNeighbors += correctNeighbors;
            sumKnownOfClosest += knownOfClosest;
        }
    }

    const double avgKbucketSize =
        static_cast<double>(sumKbucketSize) / (numNodes - 1);
    const double avgCorrectNeighbors =
        static_cast<double>(sumCorrectNeighbors) / (numNodes - 1);
    const double avgKnownOfClosest =
        static_cast<double>(sumKnownOfClosest) / (numNodes - 1);

    // TS prints these; keep them visible in the test log.
    streamr::logger::SLogger::info(
        "----------- Simulation results ------------------");
    streamr::logger::SLogger::info(
        "Minimum correct neighbors: " +
        std::to_string(minimumCorrectNeighbors));
    streamr::logger::SLogger::info(
        "Average correct neighbors: " + std::to_string(avgCorrectNeighbors));
    streamr::logger::SLogger::info(
        "Average Kbucket size: " + std::to_string(avgKbucketSize));
    streamr::logger::SLogger::info(
        "Average known-of-closest-8 (order-insensitive): " +
        std::to_string(avgKnownOfClosest));

    // Adaptation: conservative correctness floors (the TS test asserts
    // nothing). A healthy 200-node Kademlia converges most nodes to their
    // exact closest-neighbour prefix; a broken join/discovery collapses
    // these numbers to near zero.
    EXPECT_GE(avgCorrectNeighbors, 4.0);
    EXPECT_GE(avgKbucketSize, static_cast<double>(neighborsToCheck) / 2);
}
