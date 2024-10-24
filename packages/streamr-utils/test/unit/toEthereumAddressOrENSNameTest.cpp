#include <gtest/gtest.h>
#include <streamr-utils/toEthereumAddressOrENSName.hpp>

using streamr::utils::EthereumAddress;
using streamr::utils::toEthereumAddressOrENSName;

TEST(toEthereumAddressOrENSNameTest, shouldReturnEthereumAddress) {
    const std::string ethereumAddress =
        "0x1234567890123456789012345678901234567890";
    const auto expected = EthereumAddress{ethereumAddress};
    std::visit(
        [&](const auto& result) {
            if constexpr (std::is_same_v<
                              std::decay_t<decltype(result)>,
                              EthereumAddress>) {
                EXPECT_EQ(result, expected);
            } else {
                FAIL() << "Expected EthereumAddress, but got ENSName";
            }
        },
        toEthereumAddressOrENSName(ethereumAddress));
}
