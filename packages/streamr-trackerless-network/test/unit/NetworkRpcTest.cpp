// Ported from packages/trackerless-network/test/integration/
// NetworkRpc.test.ts (v103.8.0-rc.3): two raw RpcCommunicators wired
// back-to-back; the ContentDeliveryRpc client sends a stream message as
// a notification.
#include <atomic>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.protorpc.RpcCommunicator;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtCallContext;
import streamr.utils.BinaryUtils;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::RpcMessage;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::waitForCondition;
using ContentDeliveryRpcClient =
    streamr::protorpc::ContentDeliveryRpcClient<DhtCallContext>;

namespace {

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

TEST(NetworkRpcTest, SendsData) {
    RpcCommunicator<DhtCallContext> rpcCommunicator1;
    RpcCommunicator<DhtCallContext> rpcCommunicator2;
    std::atomic<int> recvCounter{0};

    rpcCommunicator1.setOutgoingMessageCallback(
        [&rpcCommunicator2](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const DhtCallContext& /* context */) {
            rpcCommunicator2.handleIncomingMessage(message, DhtCallContext());
        });
    rpcCommunicator2.registerRpcNotification<StreamMessage>(
        "sendStreamMessage",
        [&recvCounter](
            const StreamMessage& /* msg */,
            const DhtCallContext& /* context */) { recvCounter += 1; });

    ContentDeliveryRpcClient client{rpcCommunicator1};
    const auto msg = createLocalStreamMessage(
        R"({ "hello": "WORLD" })", StreamPartIDUtils::parse("testStream#0"));
    blockingWait(client.sendStreamMessage(
        StreamMessage(msg), DhtCallContext(), std::nullopt));
    blockingWait(
        waitForCondition([&recvCounter]() { return recvCounter.load() == 1; }));

    rpcCommunicator1.drainAsyncTasks();
    rpcCommunicator2.drainAsyncTasks();
}
