// Ported from packages/trackerless-network/test/unit/
// StreamPartNetworkSplitAvoidance.test.ts (v103.8.0-rc.3): each
// discoverEntryPoints call grows the mock's k-bucket by one random peer;
// the run-off runs all its attempts, so the final neighbor count exceeds
// (not merely reaches) minNeighborCount.
#include <chrono>
#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.protos;
import streamr.trackerlessnetwork.MockDiscoveryLayerNode;
import streamr.trackerlessnetwork.StreamPartNetworkSplitAvoidance;
import streamr.utils.CoroutineHelper;

using ::dht::PeerDescriptor;
using streamr::trackerlessnetwork::minNeighborCount;
using streamr::trackerlessnetwork::StreamPartNetworkSplitAvoidance;
using streamr::trackerlessnetwork::StreamPartNetworkSplitAvoidanceOptions;
using streamr::trackerlessnetwork::testutils::MockDiscoveryLayerNode;
using streamr::utils::blockingWait;

class StreamPartNetworkSplitAvoidanceTest : public ::testing::Test {
protected:
    MockDiscoveryLayerNode discoveryLayerNode;
    std::unique_ptr<StreamPartNetworkSplitAvoidance> avoidance;

    void SetUp() override {
        this->avoidance = std::make_unique<StreamPartNetworkSplitAvoidance>(
            StreamPartNetworkSplitAvoidanceOptions{
                .discoveryLayerNode = this->discoveryLayerNode,
                .discoverEntryPoints =
                    [this]() -> folly::coro::Task<std::vector<PeerDescriptor>> {
                    this->discoveryLayerNode.addNewRandomPeerToKBucket();
                    co_return this->discoveryLayerNode.getNeighbors();
                },
                .exponentialRunOfBaseDelay = std::chrono::milliseconds(1)});
    }

    void TearDown() override {
        blockingWait(this->discoveryLayerNode.stop());
        this->avoidance->destroy();
    }
};

TEST_F(
    StreamPartNetworkSplitAvoidanceTest,
    RunsAvoidanceUntilNumberOfNeighborsIsAboveMinNeighborCount) {
    blockingWait(this->avoidance->avoidNetworkSplit());
    EXPECT_GT(this->discoveryLayerNode.getNeighborCount(), minNeighborCount);
}
