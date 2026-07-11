// Ported from packages/trackerless-network/test/unit/
// TemporaryConnectionRpcLocal.test.ts (v103.8.0-rc.3): openConnection
// registers the caller in the temporary-node list, closeConnection
// removes it again.
//
// NB: NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include) — see TestUtilsTest.cpp for the clangd rationale.
#include <optional>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

import streamr.trackerlessnetwork.TemporaryConnectionRpcLocal;
import streamr.trackerlessnetwork.TestUtils;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.FakeTransport;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::FakeTransport;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::trackerlessnetwork::TemporaryConnectionRpcLocal;
using streamr::trackerlessnetwork::TemporaryConnectionRpcLocalOptions;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::trackerlessnetwork::testutils::MockConnectionLocker;
using streamr::utils::StreamPartIDUtils;

class TemporaryConnectionRpcLocalTest : public ::testing::Test {
protected:
    PeerDescriptor peerDescriptor = createMockPeerDescriptor();
    FakeTransport transport{peerDescriptor, [](const auto& /*message*/) {}};
    ListeningRpcCommunicator rpcCommunicator{ServiceID{"mock"}, transport};
    MockConnectionLocker connectionLocker;
    std::optional<TemporaryConnectionRpcLocal> rpcLocal;

    void SetUp() override {
        this->rpcLocal.emplace(
            TemporaryConnectionRpcLocalOptions{
                .localPeerDescriptor = this->peerDescriptor,
                .streamPartId = StreamPartIDUtils::parse("mock#0"),
                .rpcCommunicator = this->rpcCommunicator,
                .connectionLocker = this->connectionLocker});
    }
};

TEST_F(TemporaryConnectionRpcLocalTest, OpenAndCloseConnection) {
    const auto caller = createMockPeerDescriptor();
    const auto callerId = Identifiers::getNodeIdFromPeerDescriptor(caller);
    DhtCallContext context;
    context.incomingSourceDescriptor = caller;

    this->rpcLocal->openConnection(TemporaryConnectionRequest{}, context);
    EXPECT_TRUE(this->rpcLocal->getNodes().get(callerId).has_value());

    this->rpcLocal->closeConnection(CloseTemporaryConnection{}, context);
    EXPECT_FALSE(this->rpcLocal->getNodes().get(callerId).has_value());
}
