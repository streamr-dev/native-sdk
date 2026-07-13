// Ported from packages/trackerless-network/test/integration/
// NetworkStack.test.ts (v103.8.0-rc.3): two stacks over REAL websockets
// (no transport given — each layer-0 node owns a ConnectionManager) —
// pub/sub through the content delivery manager and joinStreamPart with
// a neighbor requirement.
//
// NB: TestUtils and the textual pb.h are avoided — this TU composes the
// full NetworkStack + DhtNode module graph and additional BMIs exhaust
// clang's per-TU source-location space (see the C3/C5 test files).
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryManager;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.PortRange;
import streamr.dht.protos;
import streamr.utils.BinaryUtils;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::types::PortRange;
using streamr::trackerlessnetwork::NeighborRequirement;
using streamr::trackerlessnetwork::NetworkOptions;
using streamr::trackerlessnetwork::NetworkStack;
using streamr::trackerlessnetwork::contentdeliverymanagerevents::NewMessage;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::waitForCondition;

namespace {

constexpr uint16_t entryPointPort = 32222;
constexpr uint16_t otherPort = 32223;

inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

StreamMessage createLocalStreamMessage(
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

class NetworkStackTest : public ::testing::Test {
protected:
    StreamPartID streamPartId = StreamPartIDUtils::parse("stream#0");
    PeerDescriptor epDescriptor;
    std::shared_ptr<NetworkStack> stack1;
    std::shared_ptr<NetworkStack> stack2;

    void SetUp() override {
        this->epDescriptor = createMockPeerDescriptor();
        this->epDescriptor.mutable_websocket()->set_host("127.0.0.1");
        this->epDescriptor.mutable_websocket()->set_port(entryPointPort);
        this->epDescriptor.mutable_websocket()->set_tls(false);

        this->stack1 = std::make_shared<NetworkStack>(NetworkOptions{
            .layer0 = DhtNodeOptions{
                .peerDescriptor = this->epDescriptor,
                .entryPoints = {this->epDescriptor}}});
        this->stack2 = std::make_shared<NetworkStack>(NetworkOptions{
            .layer0 = DhtNodeOptions{
                .entryPoints = {this->epDescriptor},
                .websocketPortRange =
                    PortRange{.min = otherPort, .max = otherPort}}});

        blockingWait(this->stack1->start());
        this->stack1->getContentDeliveryManager().setStreamPartEntryPoints(
            this->streamPartId, {this->epDescriptor});
        blockingWait(this->stack2->start());
        this->stack2->getContentDeliveryManager().setStreamPartEntryPoints(
            this->streamPartId, {this->epDescriptor});
    }

    void TearDown() override {
        if (this->stack1) {
            blockingWait(this->stack1->stop());
        }
        if (this->stack2) {
            blockingWait(this->stack2->stop());
        }
    }
};

TEST_F(NetworkStackTest, CanUseNetworkNodePubSubViaNetworkStack) {
    std::atomic<int> receivedMessages{0};
    this->stack1->getContentDeliveryManager().joinStreamPart(
        this->streamPartId);
    this->stack1->getContentDeliveryManager().on<NewMessage>(
        [&receivedMessages](const StreamMessage& /* msg */) {
            receivedMessages += 1;
        });
    const auto msg =
        createLocalStreamMessage(R"({ "hello": "WORLD" })", this->streamPartId);
    this->stack2->getContentDeliveryManager().broadcast(msg);
    blockingWait(waitForCondition(
        [&receivedMessages]() { return receivedMessages.load() == 1; },
        std::chrono::seconds(15))); // NOLINT
}

TEST_F(NetworkStackTest, JoinAndWaitForNeighbors) {
    constexpr auto neighborTimeout = std::chrono::seconds(5);
    blockingWait(
        folly::coro::collectAll(
            this->stack1->joinStreamPart(
                this->streamPartId,
                NeighborRequirement{.minCount = 1, .timeout = neighborTimeout}),
            this->stack2->joinStreamPart(
                this->streamPartId,
                NeighborRequirement{
                    .minCount = 1, .timeout = neighborTimeout})));
    EXPECT_EQ(
        this->stack1->getContentDeliveryManager()
            .getNeighbors(this->streamPartId)
            .size(),
        1);
    EXPECT_EQ(
        this->stack2->getContentDeliveryManager()
            .getNeighbors(this->streamPartId)
            .size(),
        1);
}
