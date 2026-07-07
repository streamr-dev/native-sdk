// Ported from packages/dht/test/unit/RoutingSession.test.ts
// (v103.8.0-rc.3): exercises updateAndGetRoutablePeers over the shared
// RoutingTablesCache — the routable set tracks the node's connections and
// the cached table is recalculated once it empties.
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <gtest/gtest.h>

import streamr.utils.ExecutorHelper;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RoutingSession;
import streamr.dht.RoutingTablesCache;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::PeerDescriptor;
using ::dht::RouteMessageWrapper;
using ::protorpc::RpcMessage;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::routing::RoutingMode;
using streamr::dht::routing::RoutingSession;
using streamr::dht::routing::RoutingSessionOptions;
using streamr::dht::routing::RoutingTablesCache;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::protorpc::RpcCommunicator;

namespace {
constexpr size_t parallelism = 2;
}

class RoutingSessionTest : public ::testing::Test {
protected:
    folly::CPUThreadPoolExecutor executor{1};
    RpcCommunicator<DhtCallContext> mockCommunicator;
    RoutingTablesCache routingTablesCache;
    PeerDescriptor localPeerDescriptor = createMockPeerDescriptor();
    PeerDescriptor peer = createMockPeerDescriptor();
    std::map<DhtAddress, PeerDescriptor> connections;
    std::shared_ptr<RoutingSession> session;

    [[nodiscard]] static DhtAddress nodeId(const PeerDescriptor& descriptor) {
        return Identifiers::getNodeIdFromPeerDescriptor(descriptor);
    }

    void SetUp() override {
        this->mockCommunicator.setOutgoingMessageCallback(
            [](const RpcMessage& /*message*/,
               const std::string& /*requestId*/,
               const DhtCallContext& /*context*/) {});
        RouteMessageWrapper routedMessage;
        routedMessage.set_requestid("REQ");
        routedMessage.set_target(this->localPeerDescriptor.nodeid());
        *routedMessage.mutable_sourcepeer() = createMockPeerDescriptor();
        this->session = RoutingSession::newInstance(
            RoutingSessionOptions{
                .rpcCommunicator = this->mockCommunicator,
                .localPeerDescriptor = this->localPeerDescriptor,
                .routedMessage = routedMessage,
                .parallelism = parallelism,
                .mode = RoutingMode::ROUTE,
                .excludedNodeIds = std::set<DhtAddress>{},
                .routingTablesCache = this->routingTablesCache,
                .getConnections =
                    [this]() {
                        std::vector<PeerDescriptor> result;
                        result.reserve(this->connections.size());
                        for (const auto& [id, descriptor] : this->connections) {
                            result.push_back(descriptor);
                        }
                        return result;
                    },
                .executor = &this->executor});
    }

    void TearDown() override {
        if (this->session) {
            this->session->stop();
        }
    }
};

TEST_F(RoutingSessionTest, FindMoreContacts) {
    this->connections[nodeId(peer)] = peer;
    EXPECT_EQ(this->session->updateAndGetRoutablePeers().size(), 1U);
}

TEST_F(RoutingSessionTest, FindMoreContactsPeerDisconnects) {
    this->connections[nodeId(peer)] = peer;
    EXPECT_EQ(this->session->updateAndGetRoutablePeers().size(), 1U);
    this->connections.erase(nodeId(peer));
    this->routingTablesCache.onNodeDisconnected(nodeId(peer));
    EXPECT_EQ(this->session->updateAndGetRoutablePeers().size(), 0U);
}

TEST_F(RoutingSessionTest, RecalculatesRoutingTableIfEmpty) {
    this->connections[nodeId(peer)] = peer;
    EXPECT_EQ(this->session->updateAndGetRoutablePeers().size(), 1U);
    this->routingTablesCache.onNodeDisconnected(nodeId(peer));
    EXPECT_EQ(this->session->updateAndGetRoutablePeers().size(), 1U);
}
