// Ported from packages/dht/test/integration/WebrtcConnectorRpc.test.ts
// (v103.8.0-rc.3): the four signalling notifications travel between two
// loopback-wired RpcCommunicators and hit the registered handlers.
// Adaptation: the TS test drives the raw generated client through
// toProtoRpcClient; here the calls go through WebrtcConnectorRpcRemote
// (which wraps that same generated client and is what the connector
// actually uses), awaited with blockingWait instead of until(counter).
#include <atomic>
#include <string>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.Connection;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtRpcClient;
import streamr.dht.protos;
import streamr.dht.TestUtils;
import streamr.dht.WebrtcConnectorRpcRemote;
import streamr.protorpc.RpcCommunicator;
import streamr.utils.CoroutineHelper;

using ::dht::IceCandidate;
using ::dht::PeerDescriptor;
using ::dht::RtcAnswer;
using ::dht::RtcOffer;
using ::dht::WebrtcConnectionRequest;
using ::protorpc::RpcMessage;
using streamr::dht::connection::ConnectionID;
using streamr::dht::connection::webrtc::WebrtcConnectorRpcClient;
using streamr::dht::connection::webrtc::WebrtcConnectorRpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::protorpc::RpcCommunicator;
using streamr::utils::blockingWait;

using RpcCommunicatorType = RpcCommunicator<DhtCallContext>;

class WebrtcConnectorRpcTest : public ::testing::Test {
protected:
    RpcCommunicatorType rpcCommunicator1;
    RpcCommunicatorType rpcCommunicator2;
    PeerDescriptor localPeerDescriptor = createMockPeerDescriptor();
    PeerDescriptor targetDescriptor = createMockPeerDescriptor();
    std::atomic<int> requestConnectionCounter{0};
    std::atomic<int> rtcOfferCounter{0};
    std::atomic<int> rtcAnswerCounter{0};
    std::atomic<int> iceCandidateCounter{0};

    void SetUp() override {
        this->rpcCommunicator2.registerRpcNotification<WebrtcConnectionRequest>(
            "requestConnection",
            [this](
                const WebrtcConnectionRequest& /*request*/,
                const DhtCallContext& /*context*/) {
                this->requestConnectionCounter++;
            });
        this->rpcCommunicator2.registerRpcNotification<RtcOffer>(
            "rtcOffer",
            [this](
                const RtcOffer& /*request*/,
                const DhtCallContext& /*context*/) {
                this->rtcOfferCounter++;
            });
        this->rpcCommunicator2.registerRpcNotification<RtcAnswer>(
            "rtcAnswer",
            [this](
                const RtcAnswer& /*request*/,
                const DhtCallContext& /*context*/) {
                this->rtcAnswerCounter++;
            });
        this->rpcCommunicator2.registerRpcNotification<IceCandidate>(
            "iceCandidate",
            [this](
                const IceCandidate& /*request*/,
                const DhtCallContext& /*context*/) {
                this->iceCandidateCounter++;
            });

        this->rpcCommunicator1.setOutgoingMessageCallback(
            [this](
                const RpcMessage& message,
                const std::string& /*requestId*/,
                const DhtCallContext& /*context*/) {
                this->rpcCommunicator2.handleIncomingMessage(
                    message, DhtCallContext());
            });
        this->rpcCommunicator2.setOutgoingMessageCallback(
            [this](
                const RpcMessage& message,
                const std::string& /*requestId*/,
                const DhtCallContext& /*context*/) {
                this->rpcCommunicator1.handleIncomingMessage(
                    message, DhtCallContext());
            });
    }

    [[nodiscard]] WebrtcConnectorRpcRemote createRemote() {
        return WebrtcConnectorRpcRemote(
            PeerDescriptor(this->localPeerDescriptor),
            PeerDescriptor(this->targetDescriptor),
            WebrtcConnectorRpcClient(this->rpcCommunicator1));
    }
};

TEST_F(WebrtcConnectorRpcTest, SendConnectionRequest) {
    auto remote = this->createRemote();
    blockingWait(remote.requestConnection());
    EXPECT_EQ(this->requestConnectionCounter.load(), 1);
}

TEST_F(WebrtcConnectorRpcTest, SendRtcOffer) {
    auto remote = this->createRemote();
    blockingWait(remote.sendRtcOffer("aaaaaa", ConnectionID{"rtcOffer"}));
    EXPECT_EQ(this->rtcOfferCounter.load(), 1);
}

TEST_F(WebrtcConnectorRpcTest, SendRtcAnswer) {
    auto remote = this->createRemote();
    blockingWait(remote.sendRtcAnswer("aaaaaa", ConnectionID{"rtcOffer"}));
    EXPECT_EQ(this->rtcAnswerCounter.load(), 1);
}

TEST_F(WebrtcConnectorRpcTest, SendIceCandidate) {
    auto remote = this->createRemote();
    blockingWait(remote.sendIceCandidate(
        "aaaaaa", "asdasdasdasdasd", ConnectionID{"rtcOffer"}));
    EXPECT_EQ(this->iceCandidateCounter.load(), 1);
}
