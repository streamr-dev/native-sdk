#include <gtest/gtest.h>
#include "StreamrProxyClient.hpp"

using streamrproxyclient::ErrorInvalidProxyUrl;
using streamrproxyclient::ErrorNoProxiesDefined;
using streamrproxyclient::ErrorProxyConnectionFailed;
using streamrproxyclient::StreamrProxyAddress;
using streamrproxyclient::StreamrProxyClient;
using streamrproxyclient::StreamrProxyError;
class StreamrProxyClientCppWrapperTest : public ::testing::Test {
protected:
    static constexpr const char* invalidEthereumAddress =
        "INVALID_ETHEREUM_ADDRESS";
    static constexpr const char* goodEthereumAddress =
        "0x123456789012345678901234567890123456789a";
    static constexpr const char* validEthereumAddress =
        "0x1234567890123456789012345678901234567890";
    static constexpr const char* validEthereumAddress2 =
        "0x1234567890123456789012345678901234567892";
    static constexpr const char* validEthereumAddress3 =
        "0x1234567890123456789012345678901234567893";
    static constexpr const char* invalidStreamPartId = "INVALID_STREAM_PART_ID";
    static constexpr const char* validStreamPartId =
        "0xa000000000000000000000000000000000000000#01";
    static constexpr const char* validPrivateKey =
        "0000000000000000000000000000000000000000000000000000000000000001";

    static constexpr const char* invalidProxyUrl = "poiejrg039utg240";
    static constexpr const char* validProxyUrl = "ws://valid.com";
    static constexpr const char* nonExistentProxyUrl0 = "ws://localhost:0";
    static constexpr const char* nonExistentProxyUrl1 = "ws://localhost:1";
    static constexpr const char* nonExistentProxyUrl2 = "ws://localhost:2";
};

TEST_F(StreamrProxyClientCppWrapperTest, InvalidEthereumAddressCpp) {
    EXPECT_THROW(
        {
            StreamrProxyClient client(
                invalidEthereumAddress, validPrivateKey, validStreamPartId);
        },
        StreamrProxyError);
}

TEST_F(StreamrProxyClientCppWrapperTest, InvalidStreamPartIdCpp) {
    EXPECT_THROW(
        {
            StreamrProxyClient client(
                validEthereumAddress, validPrivateKey, invalidStreamPartId);
        },
        StreamrProxyError);
}

TEST_F(StreamrProxyClientCppWrapperTest, InvalidProxyUrlCpp) {
    StreamrProxyClient client(
        validEthereumAddress, validPrivateKey, validStreamPartId);

    std::vector<StreamrProxyAddress> proxies = {
        {invalidProxyUrl, validEthereumAddress}};

    auto result = client.connect(proxies);
    EXPECT_EQ(result.successful.size(), 0);
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_TRUE(
        std::holds_alternative<ErrorInvalidProxyUrl>(result.errors[0].code));
}

TEST_F(StreamrProxyClientCppWrapperTest, NoProxiesDefinedCpp) {
    StreamrProxyClient client(
        validEthereumAddress, validPrivateKey, validStreamPartId);

    std::vector<StreamrProxyAddress> proxies;
    auto result = client.connect(proxies);

    EXPECT_EQ(result.successful.size(), 0);
    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_TRUE(
        std::holds_alternative<ErrorNoProxiesDefined>(result.errors[0].code));
}

TEST_F(StreamrProxyClientCppWrapperTest, ThreeProxyConnectionsFailedCpp) {
    StreamrProxyClient client(
        goodEthereumAddress, validPrivateKey, validStreamPartId);

    std::vector<StreamrProxyAddress> proxies = {
        {nonExistentProxyUrl0, validEthereumAddress},
        {nonExistentProxyUrl1, validEthereumAddress2},
        {nonExistentProxyUrl2, validEthereumAddress3}};

    auto result = client.connect(proxies);

    EXPECT_EQ(result.successful.size(), 0);
    EXPECT_EQ(result.errors.size(), 3);

    for (const auto& error : result.errors) {
        EXPECT_TRUE(
            std::holds_alternative<ErrorProxyConnectionFailed>(error.code));
    }

    EXPECT_EQ(result.errors[0].proxy.websocketUrl, nonExistentProxyUrl0);
    EXPECT_EQ(result.errors[1].proxy.websocketUrl, nonExistentProxyUrl1);
    EXPECT_EQ(result.errors[2].proxy.websocketUrl, nonExistentProxyUrl2);
}
