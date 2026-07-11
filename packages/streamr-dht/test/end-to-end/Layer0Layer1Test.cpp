// Ported from packages/dht/test/end-to-end/Layer0-Layer1.test.ts
// (v103.8.0-rc.3): two stream (layer-1) DHTs stacked on layer-0 nodes that
// talk over real websockets; each stream node's single closest contact must
// be the OTHER member of its stream.
#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.PortRange;
import streamr.dht.TestUtils;
import streamr.dht.protos;
import streamr.utils.CoroutineHelper;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::numberOfNodesPerKBucketDefault;
using streamr::dht::ServiceID;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::types::PortRange;
using streamr::utils::blockingWait;

namespace {
constexpr uint16_t entryPointPort = 10016;
constexpr PortRange websocketPortRange{.min = 10017, .max = 10018};
} // namespace

class Layer0Layer1Test : public ::testing::Test {
protected:
    PeerDescriptor epPeerDescriptor;
    std::shared_ptr<DhtNode> epDhtNode;
    std::shared_ptr<DhtNode> node1;
    std::shared_ptr<DhtNode> node2;
    std::shared_ptr<DhtNode> stream1Node1;
    std::shared_ptr<DhtNode> stream1Node2;
    std::shared_ptr<DhtNode> stream2Node1;
    std::shared_ptr<DhtNode> stream2Node2;

    void SetUp() override {
        this->epPeerDescriptor = createMockPeerDescriptor();
        this->epPeerDescriptor.mutable_websocket()->set_host("127.0.0.1");
        this->epPeerDescriptor.mutable_websocket()->set_port(entryPointPort);
        this->epPeerDescriptor.mutable_websocket()->set_tls(false);

        this->epDhtNode = std::make_shared<DhtNode>(
            DhtNodeOptions{.peerDescriptor = this->epPeerDescriptor});
        blockingWait(this->epDhtNode->start());
        blockingWait(this->epDhtNode->joinDht({this->epPeerDescriptor}));

        this->node1 = std::make_shared<DhtNode>(DhtNodeOptions{
            .entryPoints = {this->epPeerDescriptor},
            .websocketPortRange = websocketPortRange});
        this->node2 = std::make_shared<DhtNode>(DhtNodeOptions{
            .entryPoints = {this->epPeerDescriptor},
            .websocketPortRange = websocketPortRange});
        blockingWait(this->node1->start());
        blockingWait(this->node2->start());

        this->stream1Node1 = std::make_shared<DhtNode>(DhtNodeOptions{
            .serviceId = ServiceID{"stream1"},
            .transport = this->epDhtNode.get(),
            .connectionsView = this->epDhtNode->getConnectionsView()});
        this->stream1Node2 = std::make_shared<DhtNode>(DhtNodeOptions{
            .serviceId = ServiceID{"stream1"},
            .transport = this->node1.get(),
            .connectionsView = this->node1->getConnectionsView()});
        this->stream2Node1 = std::make_shared<DhtNode>(DhtNodeOptions{
            .serviceId = ServiceID{"stream2"},
            .transport = this->epDhtNode.get(),
            .connectionsView = this->epDhtNode->getConnectionsView()});
        this->stream2Node2 = std::make_shared<DhtNode>(DhtNodeOptions{
            .serviceId = ServiceID{"stream2"},
            .transport = this->node2.get(),
            .connectionsView = this->node2->getConnectionsView()});
        std::vector<folly::coro::Task<void>> starts;
        starts.push_back(this->stream1Node1->start());
        starts.push_back(this->stream1Node2->start());
        starts.push_back(this->stream2Node1->start());
        starts.push_back(this->stream2Node2->start());
        blockingWait(folly::coro::collectAllRange(std::move(starts)));
    }

    void TearDown() override {
        this->node1->stop();
        this->node2->stop();
        this->epDhtNode->stop();
        this->stream1Node1->stop();
        this->stream1Node2->stop();
        this->stream2Node1->stop();
        this->stream2Node2->stop();
    }
};

TEST_F(Layer0Layer1Test, HappyPath) {
    {
        std::vector<folly::coro::Task<void>> joins;
        joins.push_back(this->node1->joinDht({this->epPeerDescriptor}));
        joins.push_back(this->node2->joinDht({this->epPeerDescriptor}));
        blockingWait(folly::coro::collectAllRange(std::move(joins)));
    }
    {
        std::vector<folly::coro::Task<void>> joins;
        joins.push_back(this->stream1Node1->joinDht({this->epPeerDescriptor}));
        joins.push_back(this->stream1Node2->joinDht({this->epPeerDescriptor}));
        blockingWait(folly::coro::collectAllRange(std::move(joins)));
    }
    {
        std::vector<folly::coro::Task<void>> joins;
        joins.push_back(this->stream2Node1->joinDht({this->epPeerDescriptor}));
        joins.push_back(this->stream2Node2->joinDht({this->epPeerDescriptor}));
        blockingWait(folly::coro::collectAllRange(std::move(joins)));
    }

    const auto contacts1 =
        this->stream1Node1->getClosestContacts(numberOfNodesPerKBucketDefault);
    const auto contacts2 =
        this->stream1Node2->getClosestContacts(numberOfNodesPerKBucketDefault);
    const auto contacts3 =
        this->stream2Node1->getClosestContacts(numberOfNodesPerKBucketDefault);
    const auto contacts4 =
        this->stream2Node2->getClosestContacts(numberOfNodesPerKBucketDefault);
    ASSERT_EQ(contacts1.size(), 1);
    ASSERT_EQ(contacts2.size(), 1);
    ASSERT_EQ(contacts3.size(), 1);
    ASSERT_EQ(contacts4.size(), 1);
    EXPECT_TRUE(
        Identifiers::areEqualPeerDescriptors(
            contacts1[0], this->node1->getLocalPeerDescriptor()));
    EXPECT_TRUE(
        Identifiers::areEqualPeerDescriptors(
            contacts2[0], this->epDhtNode->getLocalPeerDescriptor()));
    EXPECT_TRUE(
        Identifiers::areEqualPeerDescriptors(
            contacts3[0], this->node2->getLocalPeerDescriptor()));
    EXPECT_TRUE(
        Identifiers::areEqualPeerDescriptors(
            contacts4[0], this->epDhtNode->getLocalPeerDescriptor()));
}
