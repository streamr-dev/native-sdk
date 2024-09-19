#include <gtest/gtest.h>
#include "streamr-dht/helpers/AddressTools.hpp"

using streamr::dht::helpers::AddressTools;

class AddressToolsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Teardown code if needed
    }
};

TEST_F(AddressToolsTest, GetAddressFromIceCandidate_ExtractIPv4AddressFromICEHostCandidate) {
    std::string candidate = "candidate:1 1 udp 2122262783 203.0.113.180 4444 typ host";
    auto result = AddressTools::getAddressFromIceCandidate(candidate);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "203.0.113.180");
}

TEST_F(AddressToolsTest, GetAddressFromIceCandidate_ExtractIPv4AddressFromICEServerReflexiveCandidate) {
    std::string candidate = "candidate:1 1 udp 2122262783 198.51.100.130 4445 typ srflx raddr 0.0.0.0 rport 0";
    auto result = AddressTools::getAddressFromIceCandidate(candidate);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "198.51.100.130");
}

TEST_F(AddressToolsTest, GetAddressFromIceCandidate_ExtractIPv6AddressFromICECandidate) {
    std::string candidate = "candidate:1 1 udp 3756231458 2001:db8::4125:918c:4402:cc54 6666 typ host";
    auto result = AddressTools::getAddressFromIceCandidate(candidate);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "2001:db8::4125:918c:4402:cc54");
}

TEST_F(AddressToolsTest, GetAddressFromIceCandidate_FailOnMDNSICECandidate) {
    std::string candidate = "candidate:1 1 udp 2122296321 9b36eaac-bb2e-49bb-bb78-21c41c499900.local 7000 typ host";
    auto result = AddressTools::getAddressFromIceCandidate(candidate);
    EXPECT_FALSE(result.has_value());
}

TEST_F(AddressToolsTest, IsPrivateIPv4_ReturnTrueForRange10_0_0_0_8) {
    EXPECT_TRUE(AddressTools::isPrivateIPv4("10.11.12.13"));  
}

TEST_F(AddressToolsTest, IsPrivateIPv4_ReturnTrueForRange172_16_0_0_12) {
    EXPECT_TRUE(AddressTools::isPrivateIPv4("172.16.130.131"));  
}

TEST_F(AddressToolsTest, IsPrivateIPv4_ReturnTrueForRange192_168_0_0_16) {
    EXPECT_TRUE(AddressTools::isPrivateIPv4("192.168.1.1"));  
}

TEST_F(AddressToolsTest, IsPrivateIPv4_ReturnFalseForAPublicAddress) {
    EXPECT_TRUE(AddressTools::isPrivateIPv4("203.0.113.181"));  
}

TEST_F(AddressToolsTest, IsPrivateIPv4_ReturnTrueForLocalhostIPAddress) {
    EXPECT_TRUE(AddressTools::isPrivateIPv4("127.0.0.1"));  
}
