
#include <gtest/gtest.h>
#include "streamr-dht/helpers/Version.hpp"

using streamr::dht::helpers::Version;

class VersionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Teardown code if needed
    }
};

TEST_F(VersionTest, Supported) {
    EXPECT_TRUE(Version::isMaybeSupportedVersion("1.0"));
    EXPECT_TRUE(Version::isMaybeSupportedVersion("1.1"));
    EXPECT_TRUE(Version::isMaybeSupportedVersion("2.0"));          
    EXPECT_TRUE(Version::isMaybeSupportedVersion("3.5"));
}

TEST_F(VersionTest, NotSupported) {
    EXPECT_FALSE(Version::isMaybeSupportedVersion(""));
    EXPECT_FALSE(Version::isMaybeSupportedVersion("100.0.0-testnet-three.3"));
    EXPECT_FALSE(Version::isMaybeSupportedVersion("0.0"));          
    EXPECT_FALSE(Version::isMaybeSupportedVersion("0.1"));
}

