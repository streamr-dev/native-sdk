// Ported from packages/trackerless-network/test/unit/
// ContentDeliveryRpcLocal.test.ts (v103.8.0-rc.3): the server side of
// content delivery invokes the duplicate check, broadcast and
// inspection-marking callbacks on sendStreamMessage, and the
// leave-notice callback on leaveStreamPartNotice.
//
// NB: NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include) — see TestUtilsTest.cpp for the clangd rationale.
#include <optional>
#include <string>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

import streamr.trackerlessnetwork.ContentDeliveryRpcLocal;
import streamr.trackerlessnetwork.TestUtils;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.FakeTransport;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;
import streamr.utils.EthereumAddress;
import streamr.utils.StreamPartID;

using ::dht::PeerDescriptor;
using streamr::dht::ServiceID;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::FakeTransport;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::trackerlessnetwork::ContentDeliveryRpcLocal;
using streamr::trackerlessnetwork::ContentDeliveryRpcLocalOptions;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::trackerlessnetwork::testutils::createStreamMessage;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::toEthereumAddress;

class ContentDeliveryRpcLocalTest : public ::testing::Test {
protected:
    PeerDescriptor peerDescriptor = createMockPeerDescriptor();
    PeerDescriptor mockSender = createMockPeerDescriptor();
    FakeTransport transport{peerDescriptor, [](const auto& /*message*/) {}};
    ListeningRpcCommunicator rpcCommunicator{
        ServiceID{"random-graph-node"}, transport};

    size_t duplicateCheckCalls = 0;
    size_t broadcastCalls = 0;
    size_t onLeaveNoticeCalls = 0;
    size_t markForInspectionCalls = 0;

    std::optional<ContentDeliveryRpcLocal> rpcLocal;

    void SetUp() override {
        this->rpcLocal.emplace(
            ContentDeliveryRpcLocalOptions{
                .localPeerDescriptor = this->peerDescriptor,
                .streamPartId = StreamPartIDUtils::parse("stream#0"),
                .markAndCheckDuplicate =
                    [this](
                        const auto& /*messageId*/, const auto& /*previous*/) {
                        this->duplicateCheckCalls++;
                        return true;
                    },
                .broadcast =
                    [this](
                        const auto& /*message*/, const auto& /*previousNode*/) {
                        this->broadcastCalls++;
                    },
                .onLeaveNotice =
                    [this](
                        const auto& /*remoteNodeId*/, bool /*isEntryPoint*/) {
                        this->onLeaveNoticeCalls++;
                    },
                .markForInspection =
                    [this](
                        const auto& /*remoteNodeId*/, const auto& /*msgId*/) {
                        this->markForInspectionCalls++;
                    },
                .rpcCommunicator = this->rpcCommunicator});
    }

    [[nodiscard]] DhtCallContext createContextFromSender() const {
        DhtCallContext context;
        context.incomingSourceDescriptor = this->mockSender;
        return context;
    }
};

TEST_F(ContentDeliveryRpcLocalTest, ServerSendStreamMessage) {
    const auto message = createStreamMessage(
        R"({"hello":"WORLD"})",
        StreamPartIDUtils::parse("random-graph#0"),
        toEthereumAddress("0x1234567890123456789012345678901234567890"));
    this->rpcLocal->sendStreamMessage(message, createContextFromSender());
    EXPECT_EQ(this->duplicateCheckCalls, 1);
    EXPECT_EQ(this->broadcastCalls, 1);
    EXPECT_EQ(this->markForInspectionCalls, 1);
}

TEST_F(ContentDeliveryRpcLocalTest, ServerLeaveStreamPartNotice) {
    LeaveStreamPartNotice leaveNotice;
    leaveNotice.set_streampartid(StreamPartIDUtils::parse("stream#0"));
    leaveNotice.set_isentrypoint(false);
    this->rpcLocal->leaveStreamPartNotice(
        leaveNotice, createContextFromSender());
    EXPECT_EQ(this->onLeaveNoticeCalls, 1);
}
