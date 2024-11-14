#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include "StreamrProxyClient.hpp"
#include "streamr-logger/SLogger.hpp"

using streamr::libstreamrproxyclient::InvalidEthereumAddress;
using streamr::libstreamrproxyclient::InvalidStreamPartId;
using streamr::libstreamrproxyclient::StreamrProxyAddress;
using streamr::libstreamrproxyclient::StreamrProxyClient;
using streamr::libstreamrproxyclient::StreamrProxyError;
using streamr::libstreamrproxyclient::StreamrProxyErrorCode;
using streamr::libstreamrproxyclient::StreamrProxyResult;
using streamr::logger::SLogger;

class StreamrProxyClientCppTest : public ::testing::Test {
protected:
    static constexpr const char* proxyWebsocketUrl = "ws://95.216.15.80:44211";
    static constexpr const char* proxyEthereumAddress =
        "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f";
    static constexpr const char* validStreamPartId2 =
        "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0";

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

    static constexpr const char* invalidProxyUrl = "poiejrg039utg240";
    static constexpr const char* validProxyUrl = "ws://valid.com";

    static constexpr const char* nonExistentProxyUrl0 = "ws://localhost:0";
    static constexpr const char* nonExistentProxyUrl1 = "ws://localhost:1";
    static constexpr const char* nonExistentProxyUrl2 = "ws://localhost:2";

    static constexpr uint64_t invalidClientHandle = 0;

    void verifyFailed(
        const StreamrProxyResult& result, StreamrProxyErrorCode expectedError) {
        // Basic result validation
        EXPECT_EQ(result.numConnected, 0);
        EXPECT_TRUE(result.successful.empty());
        EXPECT_EQ(result.failed.size(), 1);

        // Error validation
        const auto& error = result.failed[0];
        EXPECT_EQ(error.code, expectedError);
        EXPECT_FALSE(error.message.empty());
    }

    std::vector<StreamrProxyAddress> createProxyArrayFromProxy(
        const std::string& websocketUrl, const std::string& ethereumAddress) {
        return {StreamrProxyAddress{websocketUrl, ethereumAddress}};
    }

    StreamrProxyResult createClientAndConnect(
        StreamrProxyClient& client,
        std::optional<std::string> websocketUrl = std::nullopt,
        std::optional<std::string> ethereumAddress = std::nullopt) {
        std::vector<StreamrProxyAddress> proxies;
        if (websocketUrl && ethereumAddress) {
            proxies =
                createProxyArrayFromProxy(*websocketUrl, *ethereumAddress);
        }

        return client.connect(proxies);
    }

    void createClientConnectAndVerify(
        const std::string& websocketUrl,
        const std::string& ethereumAddress,
        StreamrProxyErrorCode expectedError) {
        StreamrProxyClient client(validEthereumAddress, validStreamPartId);
        StreamrProxyResult result = createClientAndConnect(
            client, std::string(websocketUrl), std::string(ethereumAddress));

        verifyFailed(result, expectedError);
    }

    void tryToCreateClientWhichFails(
        const std::string& ownEthereumAddress,
        const std::string& streamPartId,
        StreamrProxyErrorCode expectedError) {
        try {
            StreamrProxyClient client(ownEthereumAddress, streamPartId);
            FAIL() << "Expected exception with error code: "
                   << static_cast<int>(expectedError);
        } catch (const std::exception& e) {
            // Add appropriate error checking based on your exception types
            EXPECT_FALSE(std::string(e.what()).empty());
        }
    }
};

TEST_F(StreamrProxyClientCppTest, CanCreateAndDeleteProxyClient) {
    StreamrProxyClient client(validEthereumAddress, validStreamPartId);
}

TEST_F(StreamrProxyClientCppTest, InvalidEthereumAddress) {
    tryToCreateClientWhichFails(
        invalidEthereumAddress,
        validStreamPartId,
        StreamrProxyErrorCode::INVALID_ETHEREUM_ADDRESS);
}

TEST_F(StreamrProxyClientCppTest, InvalidStreamPartId) {
    tryToCreateClientWhichFails(
        validEthereumAddress,
        invalidStreamPartId,
        StreamrProxyErrorCode::INVALID_STREAM_PART_ID);
}

TEST_F(StreamrProxyClientCppTest, InvalidProxyUrl) {
    createClientConnectAndVerify(
        invalidProxyUrl,
        validEthereumAddress,
        StreamrProxyErrorCode::INVALID_PROXY_URL);
}

TEST_F(StreamrProxyClientCppTest, NoProxiesDefined) {
    StreamrProxyClient client(validEthereumAddress, validStreamPartId);
    StreamrProxyResult result = createClientAndConnect(client);
    verifyFailed(result, StreamrProxyErrorCode::NO_PROXIES_DEFINED);
}

TEST_F(StreamrProxyClientCppTest, InvalidProxyEthereumAddress) {
    createClientConnectAndVerify(
        validProxyUrl,
        invalidEthereumAddress,
        StreamrProxyErrorCode::INVALID_ETHEREUM_ADDRESS);
}

TEST_F(StreamrProxyClientCppTest, ProxyConnectionFailed) {
    createClientConnectAndVerify(
        nonExistentProxyUrl0,
        validEthereumAddress,
        StreamrProxyErrorCode::PROXY_CONNECTION_FAILED);
}

TEST_F(StreamrProxyClientCppTest, ThreeProxyConnectionsFailed) {
    StreamrProxyClient client(goodEthereumAddress, validStreamPartId);

    std::vector<StreamrProxyAddress> proxies = {
        StreamrProxyAddress{nonExistentProxyUrl0, validEthereumAddress},
        StreamrProxyAddress{nonExistentProxyUrl1, validEthereumAddress2},
        StreamrProxyAddress{nonExistentProxyUrl2, validEthereumAddress3}};

    StreamrProxyResult result = client.connect(proxies);

    EXPECT_EQ(result.numConnected, 0);
    EXPECT_TRUE(result.successful.empty());
    EXPECT_EQ(result.failed.size(), 3);

    // Verify that errors match the original proxies
    EXPECT_EQ(result.failed[0].proxy.websocketUrl, proxies[0].websocketUrl);
    EXPECT_EQ(result.failed[1].proxy.websocketUrl, proxies[1].websocketUrl);
    EXPECT_EQ(result.failed[2].proxy.websocketUrl, proxies[2].websocketUrl);

    for (const auto& error : result.failed) {
        EXPECT_EQ(error.code, StreamrProxyErrorCode::PROXY_CONNECTION_FAILED);
        EXPECT_FALSE(error.message.empty());
    }
}

TEST_F(StreamrProxyClientCppTest, ConnectSuccessfully) {
    StreamrProxyClient client(validEthereumAddress, validStreamPartId2);

    StreamrProxyResult result = createClientAndConnect(
        client,
        std::string(proxyWebsocketUrl),
        std::string(proxyEthereumAddress));

    EXPECT_EQ(result.numConnected, 1);
    EXPECT_FALSE(result.successful.empty());
    EXPECT_EQ(result.failed.size(), 0);

    const auto& successfulProxy = result.successful[0];
    EXPECT_EQ(successfulProxy.websocketUrl, proxyWebsocketUrl);
    EXPECT_EQ(successfulProxy.ethereumAddress, proxyEthereumAddress);
}

TEST_F(StreamrProxyClientCppTest, ProxyPublishWithoutConnection) {
    StreamrProxyClient client(validEthereumAddress, validStreamPartId2);
    auto publishResult = client.publish("abc", "");
    EXPECT_EQ(publishResult.numConnected, 0);
}