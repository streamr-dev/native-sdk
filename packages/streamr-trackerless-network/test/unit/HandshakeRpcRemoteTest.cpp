// Ported from packages/trackerless-network/test/integration/
// HandshakeRpcRemote.test.ts (v103.8.0-rc.3). Adaptation: the house
// two-communicator pattern instead of simulator transports (see
// ContentDeliveryRpcRemoteTest.cpp).
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
import streamr.trackerlessnetwork.HandshakeRpcRemote;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.TestUtils;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

using ::dht::PeerDescriptor;
using ::protorpc::RpcMessage;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::protorpc::RpcCommunicator;
using streamr::trackerlessnetwork::neighbordiscovery::HandshakeRpcClient;
using streamr::trackerlessnetwork::neighbordiscovery::HandshakeRpcRemote;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartIDUtils;

using RpcCommunicatorType = RpcCommunicator<DhtCallContext>;

class HandshakeRpcRemoteTest : public ::testing::Test {
protected:
    RpcCommunicatorType clientCommunicator;
    RpcCommunicatorType serverCommunicator;
    PeerDescriptor clientNode = createMockPeerDescriptor();
    PeerDescriptor serverNode = createMockPeerDescriptor();
    std::optional<HandshakeRpcRemote> rpcRemote;

    void SetUp() override {
        this->serverCommunicator.registerRpcMethod<
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
            HandshakeRpcClient(this->clientCommunicator));
    }
};

TEST_F(HandshakeRpcRemoteTest, Handshake) {
    const auto result = blockingWait(
        this->rpcRemote->handshake(StreamPartIDUtils::parse("test#0"), {}));
    EXPECT_EQ(result.accepted, true);
}
