// Ported from packages/trackerless-network/test/unit/NumberPair.test.ts
// (v103.8.0-rc.3): ordering semantics of the (a, b) pairs the duplicate
// message detector compares message references with.
#include <gtest/gtest.h>

import streamr.trackerlessnetwork.DuplicateMessageDetector;

using streamr::trackerlessnetwork::NumberPair;

TEST(NumberPairTest, EqualTo) {
    EXPECT_EQ(NumberPair(5, 2).equalTo(NumberPair(5, 3)), false);
    EXPECT_EQ(NumberPair(5, 2).equalTo(NumberPair(5, 2)), true);
}

TEST(NumberPairTest, GreaterThan) {
    EXPECT_EQ(NumberPair(5, 2).greaterThan(NumberPair(6, 2)), false);
    EXPECT_EQ(NumberPair(5, 2).greaterThan(NumberPair(5, 3)), false);
    EXPECT_EQ(NumberPair(5, 2).greaterThan(NumberPair(5, 2)), false);
    EXPECT_EQ(NumberPair(5, 2).greaterThan(NumberPair(5, 1)), true);
    EXPECT_EQ(NumberPair(5, 2).greaterThan(NumberPair(3, 2)), true);
}

TEST(NumberPairTest, GreaterThanOrEqual) {
    EXPECT_EQ(NumberPair(5, 2).greaterThanOrEqual(NumberPair(6, 2)), false);
    EXPECT_EQ(NumberPair(5, 2).greaterThanOrEqual(NumberPair(5, 3)), false);
    EXPECT_EQ(NumberPair(5, 2).greaterThanOrEqual(NumberPair(5, 2)), true);
    EXPECT_EQ(NumberPair(5, 2).greaterThanOrEqual(NumberPair(5, 1)), true);
    EXPECT_EQ(NumberPair(5, 2).greaterThanOrEqual(NumberPair(3, 2)), true);
}
