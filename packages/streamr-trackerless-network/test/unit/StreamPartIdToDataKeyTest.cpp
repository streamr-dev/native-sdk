// Ported from packages/trackerless-network/test/unit/
// StreamPartIDDataKey.test.ts (v103.8.0-rc.3): the DHT data key derived
// from a stream part id is a 160-bit (40 hex character) address.
// The exact-value assertion is a C++ addition: the key must match the
// TS derivation byte for byte (SHA1 of the stream part id), or C++ and
// TS nodes would store the same stream part's entry points under
// different DHT keys.
#include <gtest/gtest.h>

import streamr.trackerlessnetwork.streamPartIdToDataKey;
import streamr.utils.StreamPartID;

using streamr::trackerlessnetwork::streamPartIdToDataKey;
using streamr::utils::StreamPartIDUtils;

TEST(StreamPartIdToDataKeyTest, GeneratedKeyLengthIsCorrect) {
    const auto streamPartId = StreamPartIDUtils::parse("stream#0");
    const auto dataKey = streamPartIdToDataKey(streamPartId);
    EXPECT_EQ(dataKey.length(), 40);
    // sha1("stream#0") — pins wire compatibility with the TS derivation.
    EXPECT_EQ(dataKey, "23a32b02463a236285eb026d76657155b2ccb1f1");
}
