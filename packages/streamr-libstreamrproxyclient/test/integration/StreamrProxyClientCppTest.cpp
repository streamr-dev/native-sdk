#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include "StreamrProxyClient.hpp"
#include "streamr-logger/SLogger.hpp"

using streamr::libstreamrproxyclient::StreamrProxyClient;
using streamr::libstreamrproxyclient::StreamrProxyAddress;  
using streamr::libstreamrproxyclient::StreamrProxyResult;
using streamr::libstreamrproxyclient::StreamrProxyError;
using streamr::libstreamrproxyclient::StreamrProxyErrorCode;
using streamr::logger::SLogger;
using streamr::libstreamrproxyclient::InvalidEthereumAddress;
using streamr::libstreamrproxyclient::InvalidStreamPartId;
class StreamrProxyClientCppTest : public ::testing::Test {
protected:


    static constexpr const char* proxyWebsocketUrl = 
        "ws://95.216.15.80:44211";
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

public:
    // ~StreamrProxyClientCppTest() override { proxyClientCleanupLibrary(); }
};

TEST_F(StreamrProxyClientCppTest, CanCreateAndDeleteProxyClient) {
    StreamrProxyClient client(validEthereumAddress, validStreamPartId);
}

TEST_F(StreamrProxyClientCppTest, InvalidEthereumAddress) {
    try {
        StreamrProxyClient client(invalidEthereumAddress, validStreamPartId);
        FAIL() << "Expected InvalidEthereumAddress exception";
    }
    catch(const InvalidEthereumAddress& e) {
        EXPECT_EQ(e.code, StreamrProxyErrorCode::INVALID_ETHEREUM_ADDRESS);
        EXPECT_FALSE(e.message.empty());
    }
}

TEST_F(StreamrProxyClientCppTest, InvalidStreamPartId) {
    try {
        StreamrProxyClient client(validEthereumAddress, invalidStreamPartId);
        FAIL() << "Expected InvalidStreamPartId exception";
    }
    catch(const InvalidStreamPartId& e) {
        EXPECT_EQ(e.code, StreamrProxyErrorCode::INVALID_STREAM_PART_ID);
        EXPECT_FALSE(e.message.empty());
    }
}

TEST_F(StreamrProxyClientCppTest, InvalidProxyUrl) {
    // Create invalid client with invalid handle
    StreamrProxyClient client(validEthereumAddress, validStreamPartId);

    // Create a test proxy
    std::vector<StreamrProxyAddress> proxies = {
        StreamrProxyAddress{invalidProxyUrl, validEthereumAddress}};

    // Try to connect and expect error
    StreamrProxyResult result = client.connect(proxies);

    // Verify results
    EXPECT_EQ(result.numConnected, 0);
    EXPECT_TRUE(result.successful.empty());
    EXPECT_EQ(result.failed.size(), 1);

    // Check the error details
    const auto& error = result.failed[0];
    EXPECT_EQ(error.code, StreamrProxyErrorCode::INVALID_PROXY_URL);
    EXPECT_FALSE(error.message.empty());
}

TEST_F(StreamrProxyClientCppTest, NoProxiesDefined) noexcept(false) {

   // Create client with valid parameters
    StreamrProxyClient client(validEthereumAddress, validStreamPartId);

    // Try to connect with empty proxy list
    std::vector<StreamrProxyAddress> proxies;  // empty vector
    StreamrProxyResult result = client.connect(proxies);

    // Verify results
    EXPECT_EQ(result.numConnected, 0);
    EXPECT_TRUE(result.successful.empty());
    EXPECT_EQ(result.failed.size(), 1);

    // Check the error details
    const auto& error = result.failed[0];
    EXPECT_EQ(error.code, StreamrProxyErrorCode::NO_PROXIES_DEFINED);
    EXPECT_FALSE(error.message.empty());
}

TEST_F(StreamrProxyClientCppTest, InvalidProxyEthereumAddress) noexcept(false) {
    // Create client with valid parameters
    StreamrProxyClient client(validEthereumAddress, validStreamPartId);

    // Create a test proxy with invalid ethereum address
    std::vector<StreamrProxyAddress> proxies = {
        StreamrProxyAddress{
            validProxyUrl,
            invalidEthereumAddress
        }
    };

    // Try to connect and expect error
    StreamrProxyResult result = client.connect(proxies);

    // Verify results
    EXPECT_EQ(result.numConnected, 0);
    EXPECT_TRUE(result.successful.empty());
    EXPECT_EQ(result.failed.size(), 1);

    // Check the error details
    const auto& error = result.failed[0];
    EXPECT_EQ(error.code, StreamrProxyErrorCode::INVALID_ETHEREUM_ADDRESS);
    EXPECT_FALSE(error.message.empty());
}

TEST_F(StreamrProxyClientCppTest, ProxyConnectionFailed) noexcept(false) {
    // Create client with valid parameters
    StreamrProxyClient client(validEthereumAddress, validStreamPartId);

    // Create a test proxy with non-existent URL
    std::vector<StreamrProxyAddress> proxies = {
        StreamrProxyAddress{
            nonExistentProxyUrl0,
            validEthereumAddress
        }
    };

    // Try to connect and expect error
    StreamrProxyResult result = client.connect(proxies);
  // Log the error details
    SLogger::info("numErrors: " + std::to_string(result.failed.size()));
    if (!result.failed.empty()) {
        SLogger::info("error message: " + result.failed[0].message);
        SLogger::info("error code: " + std::to_string(static_cast<int>(result.failed[0].code)));
    }

    // Verify results
    EXPECT_EQ(result.numConnected, 0);
    EXPECT_TRUE(result.successful.empty());
    EXPECT_EQ(result.failed.size(), 1);

    // Check the error details
    const auto& error = result.failed[0];
    EXPECT_EQ(error.code, StreamrProxyErrorCode::PROXY_CONNECTION_FAILED);
    EXPECT_FALSE(error.message.empty());
}

TEST_F(StreamrProxyClientCppTest, ThreeProxyConnectionsFailed) noexcept(false) {
    // Create client with valid parameters
    StreamrProxyClient client(goodEthereumAddress, validStreamPartId);

    // Create test proxies
    std::vector<StreamrProxyAddress> proxies = {
        StreamrProxyAddress{nonExistentProxyUrl0, validEthereumAddress},
        StreamrProxyAddress{nonExistentProxyUrl1, validEthereumAddress2},
        StreamrProxyAddress{nonExistentProxyUrl2, validEthereumAddress3}
    };

    // Try to connect and expect errors
    StreamrProxyResult result = client.connect(proxies);
  // Verify results
    EXPECT_EQ(result.numConnected, 0);
    EXPECT_TRUE(result.successful.empty());
    EXPECT_EQ(result.failed.size(), 3);

    // Verify that errors match the original proxies
    EXPECT_EQ(result.failed[0].proxy.websocketUrl, proxies[0].websocketUrl);
    EXPECT_EQ(result.failed[1].proxy.websocketUrl, proxies[1].websocketUrl);
    EXPECT_EQ(result.failed[2].proxy.websocketUrl, proxies[2].websocketUrl);

    // Check each error
    for (const auto& error : result.failed) {
        SLogger::info("error: " + error.message);
        EXPECT_EQ(error.code, StreamrProxyErrorCode::PROXY_CONNECTION_FAILED);
    }
}


TEST_F(StreamrProxyClientCppTest, ConnectSuccessfully) noexcept(false) {
    // Create client with valid address
    StreamrProxyClient client(validEthereumAddress, validStreamPartId2);

    // Create a test proxy
    std::vector<StreamrProxyAddress> proxies = {
        StreamrProxyAddress{
            proxyWebsocketUrl,
            proxyEthereumAddress
        }
    };

    // Try to connect
    StreamrProxyResult result = client.connect(proxies);

    // Verify results
    EXPECT_EQ(result.numConnected, 1);
    EXPECT_FALSE(result.successful.empty());
    EXPECT_EQ(result.failed.size(), 0);

    // Verify proxy details
    const auto& successfulProxy = result.successful[0];
    EXPECT_EQ(successfulProxy.websocketUrl, proxyWebsocketUrl);
    EXPECT_EQ(successfulProxy.ethereumAddress, proxyEthereumAddress);
}

TEST_F(StreamrProxyClientCppTest, ProxyPublishWithoutConnection) noexcept(false) {
    // Create client
    StreamrProxyClient client(validEthereumAddress, validStreamPartId2);
    std::cout << "Created client" << std::endl;
   
    auto publishResult = client.publish("abc", "");
    std::cout << "Published message" << std::endl;
    // Verify publish results
    EXPECT_EQ(publishResult.failed.size(), 1);
    EXPECT_EQ(publishResult.numConnected, 0);
}
