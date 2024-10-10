#include "streamr-dht/dht/routing/DuplicateDetector.hpp"
#include <format>
#include <gtest/gtest.h>

using streamr::dht::routing::DuplicateDetector;

class DuplicateDetectorTest : public ::testing::Test {
protected:
    const int maxValueCount = 10;

    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Teardown code if needed
    }
};

TEST_F(DuplicateDetectorTest, DetectsDuplicates) {
    DuplicateDetector detector(maxValueCount);

    detector.add("test");
    EXPECT_EQ(detector.size(), 1);
    EXPECT_TRUE(detector.isMostLikelyDuplicate("test"));
}

TEST_F(DuplicateDetectorTest, RemovesFromTailWhenFull) {
    DuplicateDetector detector(maxValueCount);
    for (int i = 0; i < maxValueCount; i++) {
        detector.add(std::format("test{}", i));
    }
    for (int i = 0; i < maxValueCount; i++) {
        EXPECT_TRUE(detector.isMostLikelyDuplicate(std::format("test{}", i)));
    }
    detector.add("test10");
    EXPECT_EQ(detector.size(), 10);
    EXPECT_FALSE(detector.isMostLikelyDuplicate("test0"));
    EXPECT_TRUE(detector.isMostLikelyDuplicate("test10"));
}
