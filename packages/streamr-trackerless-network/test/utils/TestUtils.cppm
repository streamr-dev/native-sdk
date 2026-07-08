// Module streamr.trackerlessnetwork.TestUtils
// Test-only helpers shared by this package's test targets. Ported from
// packages/trackerless-network/test/utils/utils.ts (v103.8.0-rc.3);
// only the small factories the current tests need are here — the larger
// ones (createMockContentDeliveryLayerNodeAndDhtNode etc.) are ported by
// the plan phase that first needs them
// (trackerless-network-completion-plan.md, phase 0.2).
module;
#include <new>

#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.TestUtils;

import std;

import streamr.dht.ConnectionLocker;
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
    std::optional<std::int64_t> timestamp = std::nullopt,
    std::optional<std::int32_t> sequenceNumber = std::nullopt) {
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
    [[nodiscard]] std::size_t getLocalLockedConnectionCount() override { return 0; }
    [[nodiscard]] std::size_t getRemoteLockedConnectionCount() override { return 0; }
    [[nodiscard]] std::size_t getWeakLockedConnectionCount() override { return 0; }
};

} // namespace streamr::trackerlessnetwork::testutils
