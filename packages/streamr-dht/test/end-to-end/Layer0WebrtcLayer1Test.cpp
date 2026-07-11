// Ported from packages/dht/test/end-to-end/Layer0Webrtc-Layer1.test.ts
// (v103.8.0-rc.3): a layer-1 DHT stacked on a layer-0 whose non-entry-point
// members are all serverless, so every layer-0 connection between them is a
// real WebRTC data channel; each layer-1 node shares its layer-0 node's id
// (the TS nodeId option).
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

using ::dht::PeerDescriptor;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::utils::blockingWait;

namespace {
constexpr uint16_t entryPointPort = 23232;
constexpr size_t nodeCount = 4;
} // namespace

class Layer0WebrtcLayer1Test : public ::testing::Test {
protected:
    PeerDescriptor entrypointDescriptor;
    std::shared_ptr<DhtNode> layer0EntryPoint;
    std::shared_ptr<DhtNode> layer1EntryPoint;
    std::vector<std::shared_ptr<DhtNode>> layer0Nodes;
    std::vector<std::shared_ptr<DhtNode>> layer1Nodes;

    void SetUp() override {
        this->entrypointDescriptor = createMockPeerDescriptor();
        this->entrypointDescriptor.mutable_websocket()->set_host("127.0.0.1");
        this->entrypointDescriptor.mutable_websocket()->set_port(
            entryPointPort);
        this->entrypointDescriptor.mutable_websocket()->set_tls(false);

        this->layer0EntryPoint = std::make_shared<DhtNode>(
            DhtNodeOptions{.peerDescriptor = this->entrypointDescriptor});
        blockingWait(this->layer0EntryPoint->start());

        std::vector<streamr::dht::DhtAddress> nodeIds;
        for (size_t i = 0; i < nodeCount; i++) {
            const auto nodeId = Identifiers::createRandomDhtAddress();
            nodeIds.push_back(nodeId);
            this->layer0Nodes.push_back(
                std::make_shared<DhtNode>(DhtNodeOptions{
                    .entryPoints = {this->entrypointDescriptor},
                    .nodeId = nodeId}));
            blockingWait(this->layer0Nodes.back()->start());
        }

        this->layer1EntryPoint = std::make_shared<DhtNode>(DhtNodeOptions{
            .serviceId = ServiceID{"layer1"},
            .transport = this->layer0EntryPoint.get(),
            .connectionsView = this->layer0EntryPoint->getConnectionsView(),
            .nodeId = Identifiers::getNodeIdFromPeerDescriptor(
                this->entrypointDescriptor)});
        blockingWait(this->layer1EntryPoint->start());
        for (size_t i = 0; i < nodeCount; i++) {
            this->layer1Nodes.push_back(
                std::make_shared<DhtNode>(DhtNodeOptions{
                    .serviceId = ServiceID{"layer1"},
                    .transport = this->layer0Nodes[i].get(),
                    .connectionsView =
                        this->layer0Nodes[i]->getConnectionsView(),
                    .nodeId = nodeIds[i]}));
            blockingWait(this->layer1Nodes.back()->start());
        }

        blockingWait(
            this->layer0EntryPoint->joinDht({this->entrypointDescriptor}));
        blockingWait(
            this->layer1EntryPoint->joinDht({this->entrypointDescriptor}));
    }

    void TearDown() override {
        this->layer0EntryPoint->stop();
        for (const auto& node : this->layer0Nodes) {
            node->stop();
        }
        this->layer1EntryPoint->stop();
        for (const auto& node : this->layer1Nodes) {
            node->stop();
        }
    }
};

TEST_F(Layer0WebrtcLayer1Test, HappyPath) {
    {
        std::vector<folly::coro::Task<void>> joins;
        joins.reserve(nodeCount);
        for (const auto& node : this->layer0Nodes) {
            joins.push_back(node->joinDht({this->entrypointDescriptor}));
        }
        blockingWait(folly::coro::collectAllRange(std::move(joins)));
    }
    for (const auto& node : this->layer1Nodes) {
        blockingWait(node->joinDht({this->entrypointDescriptor}));
    }
    for (const auto& node : this->layer1Nodes) {
        EXPECT_GE(node->getNeighborCount(), 2);
    }
}
