// Unit coverage for DhtNodeRpcLocal (module streamr.dht.DhtNodeRpcLocal):
// the server-side handlers, driven directly with a DhtCallContext carrying
// the incoming source descriptor.
#include <cstddef>
#include <optional>
#include <vector>
#include <gtest/gtest.h>

import streamr.dht.DhtNodeRpcLocal;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.dht.ringIdentifiers;
import streamr.dht.TestUtils;

using ::dht::ClosestPeersRequest;
using ::dht::ClosestRingPeersRequest;
using ::dht::LeaveNotice;
using ::dht::PeerDescriptor;
using ::dht::PingRequest;
using streamr::dht::ClosestRingPeerDescriptors;
using streamr::dht::DhtAddress;
using streamr::dht::DhtNodeRpcLocal;
using streamr::dht::DhtNodeRpcLocalOptions;
using streamr::dht::Identifiers;
using streamr::dht::contact::getRingIdRawFromPeerDescriptor;
using streamr::dht::contact::RingIdRaw;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::createMockPeerDescriptor;

namespace {

constexpr size_t batchSize = 4;

DhtCallContext contextFrom(const PeerDescriptor& source) {
    DhtCallContext context;
    context.incomingSourceDescriptor = source;
    return context;
}

} // namespace

class DhtNodeRpcLocalTest : public ::testing::Test {
protected:
    std::vector<PeerDescriptor> neighbors = {
        createMockPeerDescriptor(),
        createMockPeerDescriptor(),
        createMockPeerDescriptor()};
    std::vector<PeerDescriptor> addedContacts;
    std::vector<DhtAddress> removedContacts;
    ClosestRingPeerDescriptors ringContacts;

    DhtNodeRpcLocal local{DhtNodeRpcLocalOptions{
        .peerDiscoveryQueryBatchSize = batchSize,
        .getNeighbors = [this]() { return this->neighbors; },
        .getClosestRingContactsTo =
            [this](const RingIdRaw& /*id*/, size_t /*limit*/) {
                return this->ringContacts;
            },
        .addContact =
            [this](const PeerDescriptor& contact) {
                this->addedContacts.push_back(contact);
            },
        .removeContact =
            [this](const DhtAddress& nodeId) {
                this->removedContacts.push_back(nodeId);
            }}};
};

TEST_F(DhtNodeRpcLocalTest, GetClosestPeersReturnsNeighborsAndAddsSource) {
    const auto source = createMockPeerDescriptor();
    ClosestPeersRequest request;
    request.set_nodeid(source.nodeid());
    request.set_requestid("req-1");
    const auto response = local.getClosestPeers(request, contextFrom(source));
    EXPECT_EQ(response.peers_size(), static_cast<int>(neighbors.size()));
    EXPECT_EQ(response.requestid(), "req-1");
    ASSERT_EQ(addedContacts.size(), 1U);
    EXPECT_EQ(
        Identifiers::getNodeIdFromPeerDescriptor(addedContacts[0]),
        Identifiers::getNodeIdFromPeerDescriptor(source));
}

TEST_F(DhtNodeRpcLocalTest, GetClosestRingPeersReturnsBothSides) {
    ringContacts.left = {createMockPeerDescriptor()};
    ringContacts.right = {
        createMockPeerDescriptor(), createMockPeerDescriptor()};
    const auto source = createMockPeerDescriptor();
    ClosestRingPeersRequest request;
    request.set_ringid(getRingIdRawFromPeerDescriptor(source));
    request.set_requestid("req-2");
    const auto response =
        local.getClosestRingPeers(request, contextFrom(source));
    EXPECT_EQ(response.leftpeers_size(), 1);
    EXPECT_EQ(response.rightpeers_size(), 2);
    EXPECT_EQ(response.requestid(), "req-2");
    EXPECT_EQ(addedContacts.size(), 1U);
}

TEST_F(DhtNodeRpcLocalTest, PingEchoesRequestIdAndAddsSource) {
    const auto source = createMockPeerDescriptor();
    PingRequest request;
    request.set_requestid("ping-1");
    const auto response = local.ping(request, contextFrom(source));
    EXPECT_EQ(response.requestid(), "ping-1");
    ASSERT_EQ(addedContacts.size(), 1U);
    EXPECT_EQ(
        Identifiers::getNodeIdFromPeerDescriptor(addedContacts[0]),
        Identifiers::getNodeIdFromPeerDescriptor(source));
}

TEST_F(DhtNodeRpcLocalTest, LeaveNoticeRemovesSender) {
    const auto source = createMockPeerDescriptor();
    local.leaveNotice(LeaveNotice{}, contextFrom(source));
    ASSERT_EQ(removedContacts.size(), 1U);
    EXPECT_EQ(
        removedContacts[0], Identifiers::getNodeIdFromPeerDescriptor(source));
}
