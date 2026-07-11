// Ported from packages/dht/test/end-to-end/Layer0MixedConnectionTypes.test.ts
// (v103.8.0-rc.3): a mixed network — the entry point and two nodes run real
// websocket servers, three nodes are serverless (websocket-client towards
// the servers, WebRTC among themselves). Adaptation: the TS
// waitForEvent(transport, 'connected') gates in the first case become
// waitForCondition on getConnectionCount() >= 1 for the serverless joiners
// (the same "has connected to somebody" postcondition).
#include <chrono>
#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.DhtNode;
import streamr.dht.PortRange;
import streamr.dht.TestUtils;
import streamr.dht.protos;
import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::types::PortRange;
using streamr::utils::blockingWait;
using streamr::utils::waitForCondition;

namespace {
constexpr uint16_t entryPointPort = 11221;
constexpr PortRange websocketPortRange{.min = 11222, .max = 11223};
constexpr size_t serverNodeCount = 2;
constexpr size_t serverlessNodeCount = 3;
constexpr auto connectTimeout = std::chrono::seconds(30);
constexpr auto pollInterval = std::chrono::milliseconds(200);
constexpr size_t kBucketSize = 2;
} // namespace

class Layer0MixedConnectionTypesTest : public ::testing::Test {
protected:
    PeerDescriptor epPeerDescriptor;
    std::shared_ptr<DhtNode> epDhtNode;
    // nodes[0..1] run websocket servers, nodes[2..4] are serverless.
    std::vector<std::shared_ptr<DhtNode>> nodes;

    void SetUp() override {
        this->epPeerDescriptor = createMockPeerDescriptor();
        this->epPeerDescriptor.mutable_websocket()->set_host("127.0.0.1");
        this->epPeerDescriptor.mutable_websocket()->set_port(entryPointPort);
        this->epPeerDescriptor.mutable_websocket()->set_tls(false);

        this->epDhtNode = std::make_shared<DhtNode>(DhtNodeOptions{
            .numberOfNodesPerKBucket = kBucketSize,
            .peerDescriptor = this->epPeerDescriptor});
        blockingWait(this->epDhtNode->start());
        blockingWait(this->epDhtNode->joinDht({this->epPeerDescriptor}));

        for (size_t i = 0; i < serverNodeCount; i++) {
            this->nodes.push_back(
                std::make_shared<DhtNode>(DhtNodeOptions{
                    .entryPoints = {this->epPeerDescriptor},
                    .websocketPortRange = websocketPortRange}));
        }
        for (size_t i = 0; i < serverlessNodeCount; i++) {
            this->nodes.push_back(
                std::make_shared<DhtNode>(
                    DhtNodeOptions{.entryPoints = {this->epPeerDescriptor}}));
        }
        std::vector<folly::coro::Task<void>> starts;
        starts.reserve(this->nodes.size());
        for (const auto& node : this->nodes) {
            starts.push_back(node->start());
        }
        blockingWait(folly::coro::collectAllRange(std::move(starts)));
        blockingWait(this->epDhtNode->joinDht({this->epPeerDescriptor}));
    }

    void TearDown() override {
        this->epDhtNode->stop();
        for (const auto& node : this->nodes) {
            node->stop();
        }
    }
};

TEST_F(Layer0MixedConnectionTypesTest, TwoNonServerPeersJoinFirst) {
    {
        std::vector<folly::coro::Task<void>> joins;
        joins.push_back(this->nodes[2]->joinDht({this->epPeerDescriptor}));
        joins.push_back(this->nodes[3]->joinDht({this->epPeerDescriptor}));
        blockingWait(folly::coro::collectAllRange(std::move(joins)));
        for (size_t i : {size_t{2}, size_t{3}}) {
            auto* view = this->nodes[i]->getConnectionsView();
            EXPECT_NO_THROW(blockingWait(waitForCondition(
                [view]() { return view->getConnectionCount() >= 1; },
                connectTimeout,
                pollInterval)));
        }
    }
    {
        std::vector<folly::coro::Task<void>> joins;
        joins.push_back(this->nodes[0]->joinDht({this->epPeerDescriptor}));
        joins.push_back(this->nodes[1]->joinDht({this->epPeerDescriptor}));
        joins.push_back(this->nodes[4]->joinDht({this->epPeerDescriptor}));
        blockingWait(folly::coro::collectAllRange(std::move(joins)));
    }
    EXPECT_GE(this->nodes[0]->getNeighborCount(), 2);
    EXPECT_GE(this->nodes[1]->getNeighborCount(), 2);
    EXPECT_GE(this->nodes[2]->getNeighborCount(), 2);
    EXPECT_GE(this->nodes[3]->getNeighborCount(), 2);
    EXPECT_GE(this->nodes[4]->getNeighborCount(), 1);
}

TEST_F(Layer0MixedConnectionTypesTest, SimultaneousJoins) {
    std::vector<folly::coro::Task<void>> joins;
    joins.reserve(this->nodes.size());
    for (const auto& node : this->nodes) {
        joins.push_back(node->joinDht({this->epPeerDescriptor}));
    }
    blockingWait(folly::coro::collectAllRange(std::move(joins)));
    for (const auto& node : this->nodes) {
        EXPECT_GE(node->getNeighborCount(), 2);
    }
}
