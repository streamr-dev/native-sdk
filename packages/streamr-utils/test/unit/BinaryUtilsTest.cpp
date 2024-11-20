#include "streamr-utils/BinaryUtils.hpp"
#include <iostream>
#include <string>
#include "gtest/gtest.h"

using streamr::utils::BinaryUtils;

TEST(BinaryUtilsTest, BasicTest) {
    std::string hex = "0x1234567890123456789012345678901234567890";
    std::cout << "original hex: " << hex << "\n";
    std::string binary = BinaryUtils::hexToBinaryString(hex);
    std::cout << "converted binary: " << binary << "\n";
    std::string hex2 = BinaryUtils::binaryStringToHex(binary);
    std::cout << "converted hex: " << hex2 << "\n";
    EXPECT_EQ(hex, "0x" + hex2);
}
