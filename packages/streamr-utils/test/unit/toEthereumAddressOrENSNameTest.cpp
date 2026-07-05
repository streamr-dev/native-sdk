#include <string>
#include <variant>
#include <gtest/gtest.h>

import streamr.utils.toEthereumAddressOrENSName;
import streamr.utils.ENSName;
import streamr.utils.EthereumAddress;

using streamr::utils::EthereumAddress;
using streamr::utils::toEthereumAddressOrENSName;

TEST(toEthereumAddressOrENSNameTest, shouldReturnEthereumAddress) {
    const std::string ethereumAddress =
        "0x1234567890123456789012345678901234567890";
    const auto expected = EthereumAddress{ethereumAddress};
    // holds_alternative/get instead of std::visit with a generic lambda:
    // clangd's (still experimental) modules support misjudges generic-lambda
    // visitors as non-exhaustive in import-using files (the compiler is
    // fine with either form).
    const auto result = toEthereumAddressOrENSName(ethereumAddress);
    ASSERT_TRUE(std::holds_alternative<EthereumAddress>(result))
        << "Expected EthereumAddress, but got ENSName";
    EXPECT_EQ(std::get<EthereumAddress>(result), expected);
}
