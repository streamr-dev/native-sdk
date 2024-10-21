#include <gtest/gtest.h>
#include <streamr-utils/BinaryUtils.hpp>
#include <streamr-utils/SigningUtils.hpp>

using streamr::utils::BinaryUtils;
using streamr::utils::SigningUtils;

TEST(SigninUtilsTest, createSignature) {
    const std::string privateKeyHex =
        "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820";
    const std::string expectedSignatureHex =
        "787cd72924153c88350e808de68b68c88030cbc34d053a5c696a5893d5e6fec1687c1b6205ec99aeb3375a81bf5cb8857ae39c1b55a41b32ed6399ae8da456a61b";
    const auto payload = std::string("data-to-sign");
    const auto signature =
        SigningUtils::createSignature(payload, privateKeyHex);
    EXPECT_EQ(
        BinaryUtils::binaryStringToHex(signature).size(),
        expectedSignatureHex.size());

    EXPECT_EQ(BinaryUtils::binaryStringToHex(signature), expectedSignatureHex);
}

TEST(SigninUtilsTest, hash) {
    const std::string payloadHex =
        "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820";
    const std::string expectedHash =
        "a80293a92b21fa34b4974bb5d48a2600599e5bfc569cc73c3cb5bb21613cd9fa";
    const auto hash = BinaryUtils::binaryStringToHex(
        SigningUtils::hash(BinaryUtils::hexToBinaryString(payloadHex)));
    std::string lowerCaseHash;
    std::transform(
        hash.begin(), hash.end(), std::back_inserter(lowerCaseHash), ::tolower);
    EXPECT_EQ(hash, expectedHash);
}