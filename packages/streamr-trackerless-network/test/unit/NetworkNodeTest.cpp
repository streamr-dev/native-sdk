// Ported from packages/trackerless-network/test/unit/NetworkNode.test.ts
// (v103.8.0-rc.3): the message listener add/remove behaviour over a
// stubbed stack. The TS test injects a partial NetworkStack object; here
// a subclass stubs joinStreamPart (the stack is never started, so the
// real ContentDeliveryManager acts as the bare event emitter the TS
// test builds from EventEmitter<Events>).
#include <cstdint>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryManager;
import streamr.trackerlessnetwork.NetworkNode;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.trackerlessnetwork.protos;
import streamr.utils.BinaryUtils;
import streamr.utils.StreamPartID;

using streamr::trackerlessnetwork::NeighborRequirement;
using streamr::trackerlessnetwork::NetworkNode;
using streamr::trackerlessnetwork::NetworkOptions;
using streamr::trackerlessnetwork::NetworkStack;
using streamr::trackerlessnetwork::contentdeliverymanagerevents::NewMessage;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;

namespace {

StreamMessage createMessage(int64_t id, const StreamPartID& streamPart) {
    StreamMessage msg;
    auto* messageId = msg.mutable_messageid();
    messageId->set_streamid(StreamPartIDUtils::getStreamID(streamPart));
    messageId->set_streampartition(0);
    messageId->set_sequencenumber(0);
    messageId->set_timestamp(id);
    messageId->set_publisherid(
        BinaryUtils::hexToBinaryString(
            "0x1234567890123456789012345678901234567890"));
    messageId->set_messagechainid("messageChain0");
    msg.set_signaturetype(SignatureType::ECDSA_SECP256K1_EVM);
    msg.set_signature(BinaryUtils::hexToBinaryString("0x1234"));
    auto* contentMessage = msg.mutable_contentmessage();
    contentMessage->set_encryptiontype(EncryptionType::NONE);
    contentMessage->set_contenttype(ContentType::JSON);
    contentMessage->set_content(std::to_string(id));
    return msg;
}

class StubNetworkStack : public NetworkStack {
public:
    using NetworkStack::NetworkStack;

    folly::coro::Task<void> joinStreamPart(
        StreamPartID /* streamPartId */,
        std::optional<NeighborRequirement> /* neighborRequirement */) override {
        co_return;
    }
};

} // namespace

TEST(NetworkNodeTest, MessageListener) {
    const StreamPartID streamPart = StreamPartIDUtils::parse("stream#0");
    auto stack = std::make_shared<StubNetworkStack>(NetworkOptions{});
    NetworkNode node(stack);
    blockingWait(node.join(streamPart));

    std::vector<StreamMessage> received;
    const auto token = node.addMessageListener(
        [&received](const StreamMessage& msg) { received.push_back(msg); });

    const auto msg1 = createMessage(1, streamPart);
    const auto msg2 = createMessage(2, streamPart);
    auto& manager = stack->getContentDeliveryManager();
    manager.emit<NewMessage>(msg1);
    manager.emit<NewMessage>(msg2);
    ASSERT_EQ(received.size(), 2);
    EXPECT_EQ(received[0].messageid().timestamp(), 1);
    EXPECT_EQ(received[1].messageid().timestamp(), 2);

    node.removeMessageListener(token);
    manager.emit<NewMessage>(createMessage(3, streamPart));
    EXPECT_EQ(received.size(), 2);
}
