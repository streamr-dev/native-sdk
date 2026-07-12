// Ported from packages/trackerless-network/test/unit/
// NeighborFinder.test.ts (v103.8.0-rc.3): the finder keeps running
// rounds until the neighbor list reaches minCount, then stops itself.
#include <chrono>
#include <cstdlib>
#include <optional>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.NeighborFinder;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.TestUtils;
import streamr.dht.Identifiers;
import streamr.utils.waitForCondition;

using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::trackerlessnetwork::NodeList;
using streamr::trackerlessnetwork::neighbordiscovery::NeighborFinder;
using streamr::trackerlessnetwork::neighbordiscovery::NeighborFinderOptions;
using streamr::trackerlessnetwork::testutils::
    createMockContentDeliveryRpcRemote;
using streamr::utils::blockingWait;
using streamr::utils::waitForCondition;

namespace {
constexpr size_t minCount = 4;
constexpr size_t nearbyContactCount = 30;
constexpr size_t neighborsLimit = 15;
constexpr size_t viewLimit = 30;
constexpr std::chrono::seconds findTimeout{10};
constexpr std::chrono::seconds stopTimeout{5};
constexpr std::chrono::milliseconds findPollInterval{100};
constexpr std::chrono::milliseconds stopPollInterval{50};
} // namespace

class NeighborFinderTest : public ::testing::Test {
protected:
    DhtAddress nodeId = Identifiers::createRandomDhtAddress();
    NodeList neighbors{nodeId, neighborsLimit};
    NodeList nearbyNodeView{nodeId, nearbyContactCount};
    NodeList leftNodeView{nodeId, viewLimit};
    NodeList rightNodeView{nodeId, viewLimit};
    NodeList randomNodeView{nodeId, viewLimit};
    std::optional<NeighborFinder> neighborFinder;

    void SetUp() override {
        for (size_t i = 0; i < nearbyContactCount; i++) {
            this->nearbyNodeView.add(createMockContentDeliveryRpcRemote());
        }
        this->neighborFinder.emplace(
            NeighborFinderOptions{
                .neighbors = this->neighbors,
                .nearbyNodeView = this->nearbyNodeView,
                .leftNodeView = this->leftNodeView,
                .rightNodeView = this->rightNodeView,
                .randomNodeView = this->randomNodeView,
                .doFindNeighbors = [this](std::vector<DhtAddress> excluded)
                    -> folly::coro::Task<std::vector<DhtAddress>> {
                    const auto target =
                        this->nearbyNodeView.getRandom(excluded);
                    if (rand() % 2 == 0) { // NOLINT
                        this->neighbors.add(target.value());
                    } else {
                        excluded.push_back(
                            Identifiers::getNodeIdFromPeerDescriptor(
                                target.value()->getPeerDescriptor()));
                    }
                    co_return excluded;
                },
                .minCount = minCount});
    }

    void TearDown() override { this->neighborFinder->stop(); }
};

TEST_F(NeighborFinderTest, FindsTargetNumberOfNodes) {
    this->neighborFinder->start();
    blockingWait(waitForCondition(
        [this]() { return this->neighbors.size() >= minCount; },
        findTimeout,
        findPollInterval));
    // The finder marks itself stopped at the end of the round that
    // reached the target, so allow that round to finish.
    blockingWait(waitForCondition(
        [this]() { return !this->neighborFinder->isRunning(); },
        stopTimeout,
        stopPollInterval));
    EXPECT_EQ(this->neighborFinder->isRunning(), false);
}
