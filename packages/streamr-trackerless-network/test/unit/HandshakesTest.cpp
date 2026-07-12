// Ported from packages/trackerless-network/test/integration/
// Handshakes.test.ts (v103.8.0-rc.3): a real Handshaker (node 2) shakes
// hands with node 1 over simulator transports; node 1 accepts, rejects,
// or redirects to node 3 via the interleaving protocol.
//
// NB: NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include) — see TestUtilsTest.cpp for the clangd rationale.
#include <memory>
#include <optional>
#include <set>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.Handshaker;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.TestUtils;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::connection::simulator::SimulatorTransport;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::trackerlessnetwork::ContentDeliveryRpcClient;
using streamr::trackerlessnetwork::ContentDeliveryRpcRemote;
using streamr::trackerlessnetwork::NodeList;
using streamr::trackerlessnetwork::neighbordiscovery::Handshaker;
using streamr::trackerlessnetwork::neighbordiscovery::HandshakerOptions;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartIDUtils;

namespace {
constexpr size_t viewLimit = 10;
constexpr size_t maxNeighborCount = 4;
} // namespace

class HandshakesTest : public ::testing::Test {
protected:
    PeerDescriptor peerDescriptor1 = createMockPeerDescriptor();
    PeerDescriptor peerDescriptor2 = createMockPeerDescriptor();
    PeerDescriptor peerDescriptor3 = createMockPeerDescriptor();

    Simulator simulator{LatencyType::NONE};
    std::shared_ptr<SimulatorTransport> simulatorTransport1;
    std::shared_ptr<SimulatorTransport> simulatorTransport2;
    std::shared_ptr<SimulatorTransport> simulatorTransport3;
    std::shared_ptr<ListeningRpcCommunicator> rpcCommunicator1;
    std::shared_ptr<ListeningRpcCommunicator> rpcCommunicator2;
    std::shared_ptr<ListeningRpcCommunicator> rpcCommunicator3;

    std::optional<NodeList> neighbors;
    std::optional<NodeList> leftNodeView;
    std::optional<NodeList> rightNodeView;
    std::optional<NodeList> nodeView;
    std::set<DhtAddress> ongoingHandshakes;
    std::optional<Handshaker> handshaker;

    void SetUp() override {
        const auto streamPartId = StreamPartIDUtils::parse("stream#0");
        this->simulatorTransport1 = std::make_shared<SimulatorTransport>(
            this->peerDescriptor1, this->simulator);
        this->simulatorTransport1->start();
        this->simulatorTransport2 = std::make_shared<SimulatorTransport>(
            this->peerDescriptor2, this->simulator);
        this->simulatorTransport2->start();
        this->simulatorTransport3 = std::make_shared<SimulatorTransport>(
            this->peerDescriptor3, this->simulator);
        this->simulatorTransport3->start();
        this->rpcCommunicator1 = std::make_shared<ListeningRpcCommunicator>(
            ServiceID{streamPartId}, *this->simulatorTransport1);
        this->rpcCommunicator2 = std::make_shared<ListeningRpcCommunicator>(
            ServiceID{streamPartId}, *this->simulatorTransport2);
        this->rpcCommunicator3 = std::make_shared<ListeningRpcCommunicator>(
            ServiceID{streamPartId}, *this->simulatorTransport3);

        const auto handshakerNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor2);
        this->leftNodeView.emplace(handshakerNodeId, viewLimit);
        this->rightNodeView.emplace(handshakerNodeId, viewLimit);
        this->nodeView.emplace(handshakerNodeId, viewLimit);
        ContentDeliveryRpcClient client{*this->rpcCommunicator2};
        this->nodeView->add(
            std::make_shared<ContentDeliveryRpcRemote>(
                this->peerDescriptor2, this->peerDescriptor1, client));
        this->neighbors.emplace(handshakerNodeId, maxNeighborCount);
        this->handshaker.emplace(
            HandshakerOptions{
                .localPeerDescriptor = this->peerDescriptor2,
                .streamPartId = streamPartId,
                .neighbors = this->neighbors.value(),
                .leftNodeView = this->leftNodeView.value(),
                .rightNodeView = this->rightNodeView.value(),
                .nearbyNodeView = this->nodeView.value(),
                .randomNodeView = this->nodeView.value(),
                .rpcCommunicator = *this->rpcCommunicator2,
                .maxNeighborCount = maxNeighborCount,
                .ongoingHandshakes = this->ongoingHandshakes});
    }

    void TearDown() override {
        // The C++ ListeningRpcCommunicator has no explicit stop();
        // destruction (after the transports stop) drains it.
        this->simulatorTransport1->stop();
        this->simulatorTransport2->stop();
        this->simulatorTransport3->stop();
        this->simulator.stop();
    }

    static void registerAcceptingHandshakeHandler(
        ListeningRpcCommunicator& communicator) {
        communicator.registerRpcMethod<
            StreamPartHandshakeRequest,
            StreamPartHandshakeResponse>(
            "handshake",
            [](const StreamPartHandshakeRequest& request,
               const DhtCallContext& /*context*/) {
                StreamPartHandshakeResponse response;
                response.set_requestid(request.requestid());
                response.set_accepted(true);
                return response;
            });
    }
};

TEST_F(HandshakesTest, HandshakeAccepted) {
    registerAcceptingHandshakeHandler(*this->rpcCommunicator1);
    const auto result =
        blockingWait(this->handshaker->attemptHandshakesOnContacts({}));
    EXPECT_EQ(result.size(), 0);
    EXPECT_EQ(
        this->neighbors->has(
            Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor1)),
        true);
}

TEST_F(HandshakesTest, HandshakeRejected) {
    this->rpcCommunicator1->registerRpcMethod<
        StreamPartHandshakeRequest,
        StreamPartHandshakeResponse>(
        "handshake",
        [](const StreamPartHandshakeRequest& request,
           const DhtCallContext& /*context*/) {
            StreamPartHandshakeResponse response;
            response.set_requestid(request.requestid());
            response.set_accepted(false);
            return response;
        });
    const auto result =
        blockingWait(this->handshaker->attemptHandshakesOnContacts({}));
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(
        result[0],
        Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor1));
    EXPECT_EQ(
        this->neighbors->has(
            Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor1)),
        false);
}

TEST_F(HandshakesTest, HandshakeWithInterleaving) {
    this->rpcCommunicator1->registerRpcMethod<
        StreamPartHandshakeRequest,
        StreamPartHandshakeResponse>(
        "handshake",
        [this](
            const StreamPartHandshakeRequest& request,
            const DhtCallContext& /*context*/) {
            StreamPartHandshakeResponse response;
            response.set_requestid(request.requestid());
            response.set_accepted(true);
            *response.mutable_interleavetargetdescriptor() =
                this->peerDescriptor3;
            return response;
        });
    registerAcceptingHandshakeHandler(*this->rpcCommunicator3);
    const auto result =
        blockingWait(this->handshaker->attemptHandshakesOnContacts({}));
    EXPECT_EQ(result.size(), 0);
    EXPECT_EQ(
        this->neighbors->has(
            Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor1)),
        true);
    EXPECT_EQ(
        this->neighbors->has(
            Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor3)),
        true);
}
