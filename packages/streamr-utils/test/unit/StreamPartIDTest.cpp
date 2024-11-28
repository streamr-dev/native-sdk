#include <gtest/gtest.h>
#include <streamr-utils/StreamPartID.hpp>

using streamr::utils::StreamID;
using streamr::utils::StreamPartID;
using streamr::utils::toStreamPartID;

TEST(StreamPartIDTest, shouldReturnStreamPartID) {
    const auto streamId =
        StreamID{"0x1234567890123456789012345678901234567890"};
    const auto expected = StreamPartID{streamId + "#1"};
    EXPECT_EQ(toStreamPartID(streamId, 1), expected);
}
