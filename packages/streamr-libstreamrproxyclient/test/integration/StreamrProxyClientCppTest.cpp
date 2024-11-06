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

class StreamrProxyClientCppTest : public ::testing::Test {
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
    EXPECT_THROW(
        {
            StreamrProxyClient client(
                invalidEthereumAddress, validStreamPartId);
        },
        std::runtime_error);
}

TEST_F(StreamrProxyClientCppTest, InvalidStreamPartId) {
    EXPECT_THROW(
        {
            StreamrProxyClient client(
                validEthereumAddress, invalidStreamPartId);
        },
        std::runtime_error);
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
/*

    const ProxyResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    EXPECT_EQ(result->numErrors, 0);
    EXPECT_NE(clientHandle, 0);
    proxyClientResultDelete(result);

    const ProxyResult* result2 = nullptr;
    proxyClientDelete(&result2, clientHandle);
    SLogger::info("proxyClientDelete called");
    EXPECT_EQ(result2->numErrors, 0);
    proxyClientResultDelete(result2);

    SLogger::info("test finished");
    */
