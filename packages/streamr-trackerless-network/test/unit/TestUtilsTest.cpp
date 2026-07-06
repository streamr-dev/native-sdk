// Verifies the test utilities ported from
// packages/trackerless-network/test/utils/utils.ts (v103.8.0-rc.3).
// The TS version has no dedicated test for these factories; this file
// pins down the field mapping so later ported tests can rely on it.
//
// NB: the NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include): NetworkRpc's generated types live in the global namespace,
// and clangd 22 fails to merge global-namespace declarations that
// arrive both textually and through an imported BMI (every member call
// on them is then diagnosed ambiguous; the compiler is fine).
#include <gtest/gtest.h>

import streamr.trackerlessnetwork.protos;
import streamr.trackerlessnetwork.TestUtils;
import streamr.utils.BinaryUtils;
import streamr.utils.EthereumAddress;
import streamr.utils.StreamPartID;

using streamr::trackerlessnetwork::testutils::createStreamMessage;
using streamr::trackerlessnetwork::testutils::MockConnectionLocker;
using streamr::utils::BinaryUtils;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::toEthereumAddress;

TEST(TestUtils, CreateStreamMessageSetsAllFields) {
    const auto publisherId =
        toEthereumAddress("0x1234567890123456789012345678901234567890");
    const auto streamPartId = StreamPartIDUtils::parse("stream#7");
    const auto msg = createStreamMessage(
        "{\"hello\":1}", streamPartId, publisherId, 1000, 5);

    EXPECT_EQ(msg.messageid().streamid(), "stream");
    EXPECT_EQ(msg.messageid().streampartition(), 7);
    EXPECT_EQ(msg.messageid().timestamp(), 1000);
    EXPECT_EQ(msg.messageid().sequencenumber(), 5);
    EXPECT_EQ(
        msg.messageid().publisherid(),
        BinaryUtils::hexToBinaryString(publisherId));
    // Expected value spelled out (not "messageChain0-" + publisherId):
    // clangd 22 misresolves string concatenation that mixes the textual
    // and BMI-delivered std::string declarations in this TU.
    EXPECT_EQ(
        msg.messageid().messagechainid(),
        "messageChain0-0x1234567890123456789012345678901234567890");
    EXPECT_EQ(msg.signaturetype(), SignatureType::ECDSA_SECP256K1_EVM);
    EXPECT_EQ(msg.contentmessage().content(), "{\"hello\":1}");
    EXPECT_EQ(msg.contentmessage().contenttype(), ContentType::JSON);
    EXPECT_EQ(msg.contentmessage().encryptiontype(), EncryptionType::NONE);
}

TEST(TestUtils, CreateStreamMessageDefaultsTimestampAndSequenceNumber) {
    const auto publisherId =
        toEthereumAddress("0x1234567890123456789012345678901234567890");
    const auto streamPartId = StreamPartIDUtils::parse("stream#0");
    const auto msg = createStreamMessage("x", streamPartId, publisherId);

    EXPECT_GT(msg.messageid().timestamp(), 0);
    EXPECT_EQ(msg.messageid().sequencenumber(), 0);
}

TEST(TestUtils, MockConnectionLockerReportsZeroCounts) {
    MockConnectionLocker locker;
    EXPECT_EQ(locker.getLocalLockedConnectionCount(), 0);
    EXPECT_EQ(locker.getRemoteLockedConnectionCount(), 0);
    EXPECT_EQ(locker.getWeakLockedConnectionCount(), 0);
}
