// Ported from packages/trackerless-network/test/unit/
// HandshakeRpcLocal.test.ts (v103.8.0-rc.3): accept/reject decisions of
// the handshake server, including the interleaving branches.
//
// NB: NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include) — see TestUtilsTest.cpp for the clangd rationale.
#include <optional>
#include <set>
#include <string>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.HandshakeRpcLocal;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.TestUtils;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::trackerlessnetwork::NodeList;
using streamr::trackerlessnetwork::neighbordiscovery::HandshakeRpcLocal;
using streamr::trackerlessnetwork::neighbordiscovery::HandshakeRpcLocalOptions;
using streamr::trackerlessnetwork::testutils::
    createMockContentDeliveryRpcRemote;
using streamr::trackerlessnetwork::testutils::createMockHandshakeRpcRemote;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartIDUtils;

namespace {
constexpr size_t neighborsLimit = 10;
} // namespace

class HandshakeRpcLocalTest : public ::testing::Test {
protected:
    PeerDescriptor localPeerDescriptor = createMockPeerDescriptor();
    NodeList neighbors{
        Identifiers::getNodeIdFromPeerDescriptor(localPeerDescriptor),
        neighborsLimit};
    std::set<DhtAddress> ongoingHandshakes;
    std::set<DhtAddress> ongoingInterleaves;
    size_t handshakeWithInterleavingCalls = 0;
    std::optional<HandshakeRpcLocal> rpcLocal;

    void SetUp() override {
        this->rpcLocal.emplace(
            HandshakeRpcLocalOptions{
                .streamPartId = StreamPartIDUtils::parse("stream#0"),
                .neighbors = this->neighbors,
                .ongoingHandshakes = this->ongoingHandshakes,
                .ongoingInterleaves = this->ongoingInterleaves,
                .maxNeighborCount = 4,
                .createRpcRemote =
                    [](const PeerDescriptor& /*target*/) {
                        return createMockHandshakeRpcRemote();
                    },
                .createContentDeliveryRpcRemote =
                    [](const PeerDescriptor& /*target*/) {
                        return createMockContentDeliveryRpcRemote();
                    },
                .handshakeWithInterleaving = [this](
                                                 PeerDescriptor /*target*/,
                                                 DhtAddress /*remoteNodeId*/)
                    -> folly::coro::Task<bool> {
                    this->handshakeWithInterleavingCalls++;
                    co_return true;
                }});
    }

    static StreamPartHandshakeRequest createRequest() {
        StreamPartHandshakeRequest request;
        request.set_streampartid(StreamPartIDUtils::parse("stream#0"));
        request.set_requestid("requestId");
        return request;
    }

    static DhtCallContext createContext(const PeerDescriptor& sender) {
        DhtCallContext context;
        context.incomingSourceDescriptor = sender;
        return context;
    }
};

TEST_F(HandshakeRpcLocalTest, Handshake) {
    const auto response = this->rpcLocal->handshake(
        createRequest(), createContext(createMockPeerDescriptor()));
    EXPECT_EQ(response.accepted(), true);
    EXPECT_EQ(response.has_interleavetargetdescriptor(), false);
    EXPECT_EQ(response.requestid(), "requestId");
}

TEST_F(HandshakeRpcLocalTest, HandshakeInterleave) {
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    const auto response = this->rpcLocal->handshake(
        createRequest(), createContext(createMockPeerDescriptor()));
    EXPECT_EQ(response.accepted(), true);
    EXPECT_EQ(response.has_interleavetargetdescriptor(), true);
}

TEST_F(HandshakeRpcLocalTest, UnacceptedHandshake) {
    this->ongoingHandshakes.insert(DhtAddress{"0x2222"});
    this->ongoingHandshakes.insert(DhtAddress{"0x3333"});
    this->ongoingHandshakes.insert(DhtAddress{"0x4444"});
    this->ongoingHandshakes.insert(DhtAddress{"0x5555"});
    const auto response = this->rpcLocal->handshake(
        createRequest(), createContext(createMockPeerDescriptor()));
    EXPECT_EQ(response.accepted(), false);
}

TEST_F(HandshakeRpcLocalTest, HandshakeWithInterleavingSuccess) {
    InterleaveRequest request;
    *request.mutable_interleavetargetdescriptor() = createMockPeerDescriptor();
    const auto response = blockingWait(this->rpcLocal->interleaveRequest(
        request, createContext(createMockPeerDescriptor())));
    EXPECT_EQ(response.accepted(), true);
    EXPECT_EQ(this->handshakeWithInterleavingCalls, 1);
}

TEST_F(
    HandshakeRpcLocalTest,
    RejectsHandshakesIfInterleavingToRequestorIsOngoing) {
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    const auto requestor = createMockPeerDescriptor();
    this->ongoingInterleaves.insert(
        Identifiers::getNodeIdFromPeerDescriptor(requestor));
    const auto response =
        this->rpcLocal->handshake(createRequest(), createContext(requestor));
    EXPECT_EQ(response.accepted(), false);
}

TEST_F(HandshakeRpcLocalTest, RejectsIfTooManyInterleavingRequestsOngoing) {
    const auto interleavingPeer1 = createMockPeerDescriptor();
    const auto interleavingPeer2 = createMockPeerDescriptor();
    const auto interleavingPeer3 = createMockPeerDescriptor();
    this->neighbors.add(createMockContentDeliveryRpcRemote(interleavingPeer1));
    this->neighbors.add(createMockContentDeliveryRpcRemote(interleavingPeer2));
    this->neighbors.add(createMockContentDeliveryRpcRemote(interleavingPeer3));
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->ongoingInterleaves.insert(
        Identifiers::getNodeIdFromPeerDescriptor(interleavingPeer1));
    this->ongoingInterleaves.insert(
        Identifiers::getNodeIdFromPeerDescriptor(interleavingPeer2));
    this->ongoingInterleaves.insert(
        Identifiers::getNodeIdFromPeerDescriptor(interleavingPeer3));
    const auto response = this->rpcLocal->handshake(
        createRequest(), createContext(createMockPeerDescriptor()));
    EXPECT_EQ(response.accepted(), false);
    EXPECT_EQ(this->handshakeWithInterleavingCalls, 0);
}

TEST_F(HandshakeRpcLocalTest, RejectsIfRequestorHasMoreThanMaxNeighborCount) {
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    this->neighbors.add(createMockContentDeliveryRpcRemote());
    const auto response = this->rpcLocal->handshake(
        createRequest(), createContext(createMockPeerDescriptor()));
    EXPECT_EQ(response.accepted(), false);
    EXPECT_EQ(this->handshakeWithInterleavingCalls, 0);
}
