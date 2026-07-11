// Ported from packages/dht/test/end-to-end/Layer0.test.ts (v103.8.0-rc.3):
// five DhtNodes over REAL websocket servers on 127.0.0.1 (no transport given
// — each node creates and owns its ConnectionManager, checks its own
// connectivity and generates a signed peer descriptor), the four late nodes
// joining through the entry point. Adaptation: the jest Promise.all becomes
// folly collectAllRange over the start()/joinDht() tasks.
#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.DhtNode;
import streamr.dht.PortRange;
import streamr.dht.protos;
import streamr.utils.CoroutineHelper;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::types::PortRange;
using streamr::utils::blockingWait;

namespace {

constexpr uint16_t entryPointPort = 10011;
constexpr PortRange websocketPortRange{.min = 10012, .max = 10015};
constexpr size_t nodeCount = 4;

} // namespace

class Layer0Test : public ::testing::Test {
protected:
    std::shared_ptr<DhtNode> epDhtNode;
    std::vector<std::shared_ptr<DhtNode>> nodes;

    void SetUp() override {
        this->epDhtNode = std::make_shared<DhtNode>(DhtNodeOptions{
            .websocketHost = "127.0.0.1",
            .websocketPortRange =
                PortRange{.min = entryPointPort, .max = entryPointPort}});
        blockingWait(this->epDhtNode->start());
        blockingWait(this->epDhtNode->joinDht(
            {this->epDhtNode->getLocalPeerDescriptor()}));

        for (size_t i = 0; i < nodeCount; i++) {
            this->nodes.push_back(
                std::make_shared<DhtNode>(DhtNodeOptions{
                    .entryPoints = {this->epDhtNode->getLocalPeerDescriptor()},
                    .websocketHost = "127.0.0.1",
                    .websocketPortRange = websocketPortRange}));
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
};

TEST_F(Layer0Test, HappyPath) {
    std::vector<folly::coro::Task<void>> joins;
    joins.reserve(nodeCount);
    for (const auto& node : this->nodes) {
        joins.push_back(
            node->joinDht({this->epDhtNode->getLocalPeerDescriptor()}));
    }
    blockingWait(folly::coro::collectAllRange(std::move(joins)));

    for (const auto& node : this->nodes) {
        EXPECT_GE(node->getNeighborCount(), 2);
    }
}
