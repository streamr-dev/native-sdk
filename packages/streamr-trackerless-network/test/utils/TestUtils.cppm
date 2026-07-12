// Module streamr.trackerlessnetwork.TestUtils
// Test-only helpers shared by this package's test targets. Ported from
// packages/trackerless-network/test/utils/utils.ts (v103.8.0-rc.3);
// only the small factories the current tests need are here — the larger
// ones (createMockContentDeliveryLayerNodeAndDhtNode etc.) are ported by
// the plan phase that first needs them
// (trackerless-network-completion-plan.md, phase 0.2).
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.TestUtils;

import streamr.utils.CoroutineHelper;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.protos;
import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.HandshakeRpcRemote;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.dht.ConnectionLocker;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.utils.BinaryUtils;
import streamr.utils.EthereumAddress;
import streamr.utils.StreamPartID;

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::LockID;
using streamr::utils::BinaryUtils;
using streamr::utils::EthereumAddress;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;

export namespace streamr::trackerlessnetwork::testutils {

// Ported from createStreamMessage() (test/utils/utils.ts). The TS
// publisherId parameter is a UserID (branded hex string); the closest
// C++ counterpart is EthereumAddress.
inline StreamMessage createStreamMessage(
    const std::string& content,
    const StreamPartID& streamPartId,
    const EthereumAddress& publisherId,
    std::optional<int64_t> timestamp = std::nullopt,
    std::optional<int32_t> sequenceNumber = std::nullopt) {
    StreamMessage msg;
    auto* messageId = msg.mutable_messageid();
    messageId->set_streamid(StreamPartIDUtils::getStreamID(streamPartId));
    messageId->set_streampartition(
        StreamPartIDUtils::getStreamPartition(streamPartId).value_or(0));
    messageId->set_sequencenumber(sequenceNumber.value_or(0));
    messageId->set_timestamp(timestamp.value_or(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count()));
    messageId->set_publisherid(BinaryUtils::hexToBinaryString(publisherId));
    messageId->set_messagechainid("messageChain0-" + publisherId);

    msg.set_signaturetype(SignatureType::ECDSA_SECP256K1_EVM);
    msg.set_signature(BinaryUtils::hexToBinaryString("0x1234"));
    auto* contentMessage = msg.mutable_contentmessage();
    contentMessage->set_encryptiontype(EncryptionType::NONE);
    contentMessage->set_contenttype(ContentType::JSON);
    contentMessage->set_content(content);
    return msg;
}

// Ported from createMockPeerDescriptor() (test/utils/utils.ts): a
// descriptor with a random node id, sufficient for tests that only need
// identity (mirrors the streamr-dht test util of the same name).
inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        streamr::dht::Identifiers::getRawFromDhtAddress(
            streamr::dht::Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

// Shared transportless communicator behind the mock remote factories
// (the TS factories pass `new RpcCommunicator()` per remote). Outgoing
// messages fail immediately, so any RPC attempted on a mock remote
// reports failure fast instead of waiting for a response timeout.
// NOT leaked, deliberately: the communicator's serial executor holds a
// KeepAlive on the shared worker pool, and a leaked KeepAlive makes the
// pool's static destructor wait forever at process exit
// (DefaultKeepAliveExecutor::joinKeepAlive). As a function-local static
// it is constructed after the pool and therefore destroyed before it,
// releasing the KeepAlive in time.
inline streamr::protorpc::RpcCommunicator<
    streamr::dht::rpcprotocol::DhtCallContext>&
getMockRpcCommunicator() {
    using streamr::dht::rpcprotocol::DhtCallContext;
    static streamr::protorpc::RpcCommunicator<DhtCallContext> communicator;
    static const bool initialized = []() {
        communicator.setOutgoingMessageCallback(
            [](const ::protorpc::RpcMessage& /*message*/,
               const std::string& /*requestId*/,
               const DhtCallContext& /*context*/) {
                throw std::runtime_error(
                    "mock rpc communicator has no transport");
            });
        return true;
    }();
    (void)initialized;
    return communicator;
}

// Ported from createMockContentDeliveryRpcRemote() (test/utils/utils.ts).
inline std::shared_ptr<streamr::trackerlessnetwork::ContentDeliveryRpcRemote>
createMockContentDeliveryRpcRemote(
    std::optional<PeerDescriptor> remotePeerDescriptor = std::nullopt) {
    streamr::trackerlessnetwork::ContentDeliveryRpcClient client{
        getMockRpcCommunicator()};
    return std::make_shared<
        streamr::trackerlessnetwork::ContentDeliveryRpcRemote>(
        createMockPeerDescriptor(),
        remotePeerDescriptor.value_or(createMockPeerDescriptor()),
        client);
}

// Ported from createMockHandshakeRpcRemote() (test/utils/utils.ts).
inline std::shared_ptr<
    streamr::trackerlessnetwork::neighbordiscovery::HandshakeRpcRemote>
createMockHandshakeRpcRemote() {
    streamr::trackerlessnetwork::neighbordiscovery::HandshakeRpcClient client{
        getMockRpcCommunicator()};
    return std::make_shared<
        streamr::trackerlessnetwork::neighbordiscovery::HandshakeRpcRemote>(
        createMockPeerDescriptor(), createMockPeerDescriptor(), client);
}

// Ported from mockConnectionLocker (test/utils/utils.ts): a no-op
// ConnectionLocker for components that require one but whose locking
// behavior is irrelevant to the test.
class MockConnectionLocker : public ConnectionLocker {
public:
    ~MockConnectionLocker() override = default;

    void lockConnection(
        PeerDescriptor /*targetDescriptor*/, LockID /*lockId*/) override {}
    void unlockConnection(
        PeerDescriptor /*targetDescriptor*/, LockID /*lockId*/) override {}
    void weakLockConnection(
        const DhtAddress& /*targetDescriptor*/,
        const LockID& /*lockId*/) override {}
    void weakUnlockConnection(
        const DhtAddress& /*targetDescriptor*/,
        const LockID& /*lockId*/) override {}
    [[nodiscard]] size_t getLocalLockedConnectionCount() override { return 0; }
    [[nodiscard]] size_t getRemoteLockedConnectionCount() override { return 0; }
    [[nodiscard]] size_t getWeakLockedConnectionCount() override { return 0; }
};

} // namespace streamr::trackerlessnetwork::testutils
