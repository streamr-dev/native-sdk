// Ported from packages/dht/test/end-to-end/Layer0Webrtc.test.ts
// (v103.8.0-rc.3): the entry point serves a real websocket (given up-front
// in its peer descriptor); the four other nodes have NO websocket at all —
// they run their connectivity checks against the entry point over real
// sockets, connect to it as websocket clients, and to EACH OTHER over real
// WebRTC data channels signalled through the DHT. Adaptation: the TS
// waitForEvent('connected', matching node1) becomes waitForCondition on
// hasConnection in both directions (same postcondition the test asserts).
#include <chrono>
#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.TestUtils;
import streamr.dht.protos;
import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::utils::blockingWait;
using streamr::utils::waitForCondition;

namespace {
constexpr uint16_t entryPointPort = 10029;
constexpr size_t nodeCount = 4;
// TS gives the connected-wait 20 s within a 60 s jest budget.
constexpr auto connectTimeout = std::chrono::seconds(30);
constexpr auto pollInterval = std::chrono::milliseconds(200);
} // namespace

class Layer0WebrtcTest : public ::testing::Test {
protected:
    PeerDescriptor epPeerDescriptor;
    std::shared_ptr<DhtNode> epDhtNode;
    std::vector<std::shared_ptr<DhtNode>> nodes;

    void SetUp() override {
        this->epPeerDescriptor = createMockPeerDescriptor();
        this->epPeerDescriptor.mutable_websocket()->set_host("127.0.0.1");
        this->epPeerDescriptor.mutable_websocket()->set_port(entryPointPort);
        this->epPeerDescriptor.mutable_websocket()->set_tls(false);

        this->epDhtNode = std::make_shared<DhtNode>(
            DhtNodeOptions{.peerDescriptor = this->epPeerDescriptor});
        blockingWait(this->epDhtNode->start());
        blockingWait(this->epDhtNode->joinDht({this->epPeerDescriptor}));

        for (size_t i = 0; i < nodeCount; i++) {
            this->nodes.push_back(
                std::make_shared<DhtNode>(
                    DhtNodeOptions{.entryPoints = {this->epPeerDescriptor}}));
        }
        std::vector<folly::coro::Task<void>> starts;
        starts.reserve(nodeCount);
        for (const auto& node : this->nodes) {
            starts.push_back(node->start());
        }
        blockingWait(folly::coro::collectAllRange(std::move(starts)));
    }

    void TearDown() override {
        for (const auto& node : this->nodes) {
            node->stop();
        }
        this->epDhtNode->stop();
    }

    void expectMutuallyConnected(DhtNode& node1, DhtNode& node2) {
        const auto nodeId1 = Identifiers::getNodeIdFromPeerDescriptor(
            node1.getLocalPeerDescriptor());
        const auto nodeId2 = Identifiers::getNodeIdFromPeerDescriptor(
            node2.getLocalPeerDescriptor());
        EXPECT_NO_THROW(blockingWait(waitForCondition(
            [&node1, &node2, nodeId1, nodeId2]() {
                return node1.getConnectionsView()->hasConnection(nodeId2) &&
                    node2.getConnectionsView()->hasConnection(nodeId1);
            },
            connectTimeout,
            pollInterval)));
    }
};

TEST_F(Layer0WebrtcTest, HappyPathTwoPeers) {
    std::vector<folly::coro::Task<void>> joins;
    joins.push_back(this->nodes[0]->joinDht({this->epPeerDescriptor}));
    joins.push_back(this->nodes[1]->joinDht({this->epPeerDescriptor}));
    blockingWait(folly::coro::collectAllRange(std::move(joins)));
    this->expectMutuallyConnected(*this->nodes[0], *this->nodes[1]);
}

TEST_F(Layer0WebrtcTest, HappyPathSimultaneousJoins) {
    std::vector<folly::coro::Task<void>> joins;
    joins.reserve(nodeCount);
    for (const auto& node : this->nodes) {
        joins.push_back(node->joinDht({this->epPeerDescriptor}));
    }
    blockingWait(folly::coro::collectAllRange(std::move(joins)));
    this->expectMutuallyConnected(*this->nodes[0], *this->nodes[1]);
}
