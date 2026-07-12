// Ported from packages/trackerless-network/test/unit/Handshaker.test.ts
// (v103.8.0-rc.3): target selection with empty views and with
// unreachable contacts. Adaptation: the TS test runs over
// Simulator/SimulatorTransport; a FakeTransport whose send callback
// throws gives the same observable behavior (handshakes to the mock
// contacts fail) without waiting for RPC timeouts.
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.Handshaker;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.TestUtils;
import streamr.dht.DhtCallContext;
import streamr.dht.FakeTransport;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::transport::FakeTransport;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::trackerlessnetwork::NodeList;
using streamr::trackerlessnetwork::neighbordiscovery::Handshaker;
using streamr::trackerlessnetwork::neighbordiscovery::HandshakerOptions;
using streamr::trackerlessnetwork::testutils::
    createMockContentDeliveryRpcRemote;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartIDUtils;

namespace {
constexpr size_t maxNeighborCount = 4;
constexpr size_t neighborsLimit = 10;
constexpr size_t viewLimit = 20;
constexpr std::chrono::milliseconds rpcRequestTimeout{5000};
} // namespace

class HandshakerTest : public ::testing::Test {
protected:
    PeerDescriptor peerDescriptor = createMockPeerDescriptor();
    FakeTransport transport{peerDescriptor, [](const auto& /*message*/) {
                                throw std::runtime_error("unreachable");
                            }};
    ListeningRpcCommunicator rpcCommunicator{
        ServiceID{StreamPartIDUtils::parse("stream#0")}, transport};
    DhtAddress nodeId =
        Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor);
    NodeList neighbors{nodeId, neighborsLimit};
    NodeList leftNodeView{nodeId, viewLimit};
    NodeList rightNodeView{nodeId, viewLimit};
    NodeList nearbyNodeView{nodeId, viewLimit};
    NodeList randomNodeView{nodeId, viewLimit};
    std::set<DhtAddress> ongoingHandshakes;
    std::optional<Handshaker> handshaker;

    void SetUp() override {
        this->handshaker.emplace(
            HandshakerOptions{
                .localPeerDescriptor = this->peerDescriptor,
                .streamPartId = StreamPartIDUtils::parse("stream#0"),
                .neighbors = this->neighbors,
                .leftNodeView = this->leftNodeView,
                .rightNodeView = this->rightNodeView,
                .nearbyNodeView = this->nearbyNodeView,
                .randomNodeView = this->randomNodeView,
                .rpcCommunicator = this->rpcCommunicator,
                .maxNeighborCount = maxNeighborCount,
                .ongoingHandshakes = this->ongoingHandshakes,
                .rpcRequestTimeout = rpcRequestTimeout});
    }
};

TEST_F(HandshakerTest, AttemptHandshakesOnContactWorksWithEmptyStructures) {
    const auto result =
        blockingWait(this->handshaker->attemptHandshakesOnContacts({}));
    EXPECT_EQ(result.size(), 0);
}

TEST_F(HandshakerTest, AttemptHandshakesWithKnownNodesThatCannotBeConnectedTo) {
    this->randomNodeView.add(createMockContentDeliveryRpcRemote());
    this->randomNodeView.add(createMockContentDeliveryRpcRemote());
    const auto result =
        blockingWait(this->handshaker->attemptHandshakesOnContacts({}));
    EXPECT_EQ(result.size(), 2);
}
