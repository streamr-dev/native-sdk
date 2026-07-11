// Ported from packages/trackerless-network/test/integration/
// ContentDeliveryRpcRemote.test.ts (v103.8.0-rc.3): the client side of
// content delivery reaches a server's registered notification handlers.
// Adaptation: the TS test wires the two ListeningRpcCommunicators over
// SimulatorTransports; the C++ house pattern for RPC-remote tests wires
// two plain RpcCommunicators back to back (see streamr-dht's
// DhtNodeRpcRemoteTest), which exercises the same client/server RPC
// path without the simulator moving parts.
//
// NB: NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include) — see TestUtilsTest.cpp for the clangd rationale.
#include <optional>
#include <string>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.protos;
import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.TestUtils;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.protos;
import streamr.utils.EthereumAddress;
import streamr.utils.StreamPartID;

using ::dht::PeerDescriptor;
using ::protorpc::RpcMessage;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::protorpc::RpcCommunicator;
using streamr::trackerlessnetwork::ContentDeliveryRpcRemote;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::trackerlessnetwork::testutils::createStreamMessage;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::toEthereumAddress;

using ContentDeliveryRpcClient =
    streamr::protorpc::ContentDeliveryRpcClient<DhtCallContext>;
using RpcCommunicatorType = RpcCommunicator<DhtCallContext>;

class ContentDeliveryRpcRemoteTest : public ::testing::Test {
protected:
    RpcCommunicatorType clientCommunicator;
    RpcCommunicatorType serverCommunicator;
    PeerDescriptor clientNode = createMockPeerDescriptor();
    PeerDescriptor serverNode = createMockPeerDescriptor();
    size_t recvCounter = 0;
    std::optional<ContentDeliveryRpcRemote> rpcRemote;

    void SetUp() override {
        this->serverCommunicator.registerRpcNotification<StreamMessage>(
            "sendStreamMessage",
            [this](
                const StreamMessage& /*message*/,
                const DhtCallContext& /*context*/) { this->recvCounter++; });
        this->serverCommunicator.registerRpcNotification<LeaveStreamPartNotice>(
            "leaveStreamPartNotice",
            [this](
                const LeaveStreamPartNotice& /*message*/,
                const DhtCallContext& /*context*/) { this->recvCounter++; });
        this->clientCommunicator.setOutgoingMessageCallback(
            [this](
                const RpcMessage& message,
                const std::string& /*requestId*/,
                const DhtCallContext& /*context*/) {
                this->serverCommunicator.handleIncomingMessage(
                    message, DhtCallContext());
            });
        this->serverCommunicator.setOutgoingMessageCallback(
            [this](
                const RpcMessage& message,
                const std::string& /*requestId*/,
                const DhtCallContext& /*context*/) {
                this->clientCommunicator.handleIncomingMessage(
                    message, DhtCallContext());
            });
        this->rpcRemote.emplace(
            this->clientNode,
            this->serverNode,
            ContentDeliveryRpcClient(this->clientCommunicator));
    }
};

TEST_F(ContentDeliveryRpcRemoteTest, SendStreamMessage) {
    const auto msg = createStreamMessage(
        R"({"hello":"WORLD"})",
        StreamPartIDUtils::parse("test-stream#0"),
        toEthereumAddress("0x1234567890123456789012345678901234567890"));
    blockingWait(this->rpcRemote->sendStreamMessage(msg));
    EXPECT_EQ(this->recvCounter, 1);
}

TEST_F(ContentDeliveryRpcRemoteTest, LeaveNotice) {
    blockingWait(this->rpcRemote->leaveStreamPartNotice(
        StreamPartIDUtils::parse("test#0"), false));
    EXPECT_EQ(this->recvCounter, 1);
}
