// Ported from packages/trackerless-network/test/end-to-end/
// webrtc-full-node-network.test.ts (v103.8.0-rc.3): 22 NetworkStacks
// WITHOUT websocket servers (their connections form over WebRTC, with
// the entry point's websocket used only for signalling) form one
// stream-part overlay; a message broadcast by the entry point reaches
// every node.
//
// NB: TestUtils and the textual pb.h are avoided — this TU composes the
// full NetworkStack + DhtNode module graph and additional BMIs exhaust
// clang's per-TU source-location space (see the C3/C5 test files).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryManager;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.utils.BinaryUtils;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::trackerlessnetwork::NetworkOptions;
using streamr::trackerlessnetwork::NetworkStack;
using streamr::trackerlessnetwork::contentdeliverymanagerevents::NewMessage;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::waitForCondition;

namespace {

// TS parity: 22 nodes start concurrently against one entry point.
constexpr size_t numOfNodes = 22;
constexpr uint16_t entryPointPort = 14444;
// Generous for the 3-core CI runners: the TS test tolerates up to its
// 220 s jest budget for convergence.
constexpr auto neighborTimeout = std::chrono::seconds(90);
constexpr auto messageTimeout = std::chrono::seconds(15);

inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

StreamMessage createTestMessage(
    const std::string& content, const StreamPartID& streamPartId) {
    StreamMessage msg;
    auto* messageId = msg.mutable_messageid();
    messageId->set_streamid(StreamPartIDUtils::getStreamID(streamPartId));
    messageId->set_streampartition(
        static_cast<int32_t>(
            StreamPartIDUtils::getStreamPartition(streamPartId).value_or(0)));
    messageId->set_sequencenumber(0);
    messageId->set_timestamp(666); // NOLINT
    messageId->set_publisherid(
        BinaryUtils::hexToBinaryString(
            "0x1234567890123456789012345678901234567890"));
    messageId->set_messagechainid("messageChain0");
    msg.set_signaturetype(SignatureType::ECDSA_SECP256K1_EVM);
    msg.set_signature(BinaryUtils::hexToBinaryString("0x1234"));
    auto* contentMessage = msg.mutable_contentmessage();
    contentMessage->set_encryptiontype(EncryptionType::NONE);
    contentMessage->set_contenttype(ContentType::JSON);
    contentMessage->set_content(content);
    return msg;
}

} // namespace

class WebrtcFullNodeNetworkTest : public ::testing::Test {
protected:
    StreamPartID streamPartId = StreamPartIDUtils::parse("webrtc-network#0");
    std::shared_ptr<NetworkStack> entryPoint;
    std::vector<std::shared_ptr<NetworkStack>> nodes;

    void SetUp() override {
        auto epPeerDescriptor = createMockPeerDescriptor();
        epPeerDescriptor.mutable_websocket()->set_host("127.0.0.1");
        epPeerDescriptor.mutable_websocket()->set_port(entryPointPort);
        epPeerDescriptor.mutable_websocket()->set_tls(false);

        this->entryPoint = std::make_shared<NetworkStack>(NetworkOptions{
            .layer0 = DhtNodeOptions{
                .peerDescriptor = epPeerDescriptor,
                .entryPoints = {epPeerDescriptor}}});
        blockingWait(this->entryPoint->start());
        this->entryPoint->getContentDeliveryManager().joinStreamPart(
            this->streamPartId);

        // TS parity: the nodes start CONCURRENTLY (Promise.all); see the
        // websocket variant for why sequential starts overload the entry
        // point. No websocket in the descriptors: the nodes' connections
        // form over WebRTC.
        for (size_t i = 0; i < numOfNodes; i++) {
            const auto peerDescriptor = createMockPeerDescriptor();
            this->nodes.push_back(
                std::make_shared<NetworkStack>(NetworkOptions{
                    .layer0 = DhtNodeOptions{
                        .peerDescriptor = peerDescriptor,
                        .entryPoints = {epPeerDescriptor}}}));
        }
        std::vector<folly::coro::Task<void>> starts;
        starts.reserve(numOfNodes);
        for (const auto& node : this->nodes) {
            starts.push_back(node->start());
        }
        blockingWait(folly::coro::collectAllRange(std::move(starts)));
        for (const auto& node : this->nodes) {
            node->getContentDeliveryManager().joinStreamPart(
                this->streamPartId);
        }
    }

    // Null-guarded: when SetUp() throws (e.g. the websocket port is
    // still in TIME_WAIT), gtest still runs TearDown() with later
    // members never assigned.
    void TearDown() override {
        // TS parity (Promise.all in afterEach) and a hard requirement on
        // slow runners: stopping the stacks SEQUENTIALLY after a failed
        // convergence stacks up each node's bounded stop timeouts into
        // many minutes (seen as a 19-minute TearDown on the linux-arm64
        // CI runner); concurrent stops bound it by the slowest stack.
        std::vector<folly::coro::Task<void>> stops;
        for (const auto& node : this->nodes) {
            if (node) {
                stops.push_back(node->stop());
            }
        }
        if (this->entryPoint) {
            stops.push_back(this->entryPoint->stop());
        }
        blockingWait(folly::coro::collectAllRange(std::move(stops)));
    }
};

TEST_F(WebrtcFullNodeNetworkTest, HappyPath) {
    blockingWait(waitForCondition(
        [this]() {
            return std::ranges::all_of(this->nodes, [this](const auto& node) {
                return node->getContentDeliveryManager()
                           .getNeighbors(this->streamPartId)
                           .size() >= 3;
            });
        },
        neighborTimeout));

    std::atomic<size_t> receivedMessageCount{0};
    for (const auto& node : this->nodes) {
        node->getContentDeliveryManager().on<NewMessage>(
            [&receivedMessageCount](const StreamMessage& /* msg */) {
                receivedMessageCount += 1;
            });
    }

    const auto msg =
        createTestMessage(R"({ "hello": "WORLD" })", this->streamPartId);
    this->entryPoint->getContentDeliveryManager().broadcast(msg);
    blockingWait(waitForCondition(
        [&receivedMessageCount]() {
            return receivedMessageCount.load() == numOfNodes;
        },
        messageTimeout));
}
