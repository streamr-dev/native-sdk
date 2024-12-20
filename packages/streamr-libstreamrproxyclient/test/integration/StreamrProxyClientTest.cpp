#include "streamrproxyclient.h"
#include <iostream>
#include <string>
#include <gtest/gtest.h>
#include "streamr-logger/SLogger.hpp"

using streamr::logger::SLogger;

class StreamrProxyClientTest : public ::testing::Test {
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
    ~StreamrProxyClientTest() override { proxyClientCleanupLibrary(); }
};

TEST_F(StreamrProxyClientTest, CanCallApi) {
    std::cout << "RPC returned: " << testRpc() << "\n";
}

TEST_F(StreamrProxyClientTest, CanCreateAndDeleteProxyClient) {
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
}

TEST_F(StreamrProxyClientTest, InvalidEthereumAddress) {
    const ProxyResult* result = nullptr;

    const char* ownEthereumAddress = invalidEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    SLogger::info("numErrors: " + std::to_string(result->numErrors));
    SLogger::info("errors: " + std::string(result->errors->message));
    SLogger::info("errors: " + std::string(result->errors->code));

    EXPECT_EQ(result->numErrors, 1);
    EXPECT_STREQ(result->errors->code, ERROR_INVALID_ETHEREUM_ADDRESS);
    proxyClientResultDelete(result);
}

TEST_F(StreamrProxyClientTest, InvalidStreamPartId) {
    const ProxyResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = invalidStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    SLogger::info("numErrors: " + std::to_string(result->numErrors));
    SLogger::info("errors: " + std::string(result->errors->message));
    SLogger::info("errors: " + std::string(result->errors->code));

    EXPECT_EQ(result->numErrors, 1);
    EXPECT_STREQ(result->errors->code, ERROR_INVALID_STREAM_PART_ID);
    proxyClientResultDelete(result);
}

TEST_F(StreamrProxyClientTest, ProxyClientNotFound) {
    const ProxyResult* result = nullptr;

    Proxy proxy{
        .websocketUrl = invalidProxyUrl,
        .ethereumAddress = validEthereumAddress};

    proxyClientConnect(&result, invalidClientHandle, &proxy, 1);

    EXPECT_EQ(result->numErrors, 1);
    EXPECT_STREQ(result->errors->code, ERROR_PROXY_CLIENT_NOT_FOUND);
    proxyClientResultDelete(result);
}

TEST_F(StreamrProxyClientTest, NoProxiesDefined) {
    const ProxyResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    proxyClientConnect(&result, clientHandle, nullptr, 0);

    EXPECT_EQ(result->numErrors, 1);
    EXPECT_STREQ(result->errors->code, ERROR_NO_PROXIES_DEFINED);
    proxyClientResultDelete(result);
}

TEST_F(StreamrProxyClientTest, InvalidProxyUrl) {
    const ProxyResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    proxyClientResultDelete(result);

    const ProxyResult* result2 = nullptr;
    Proxy proxy{
        .websocketUrl = invalidProxyUrl,
        .ethereumAddress = validEthereumAddress};
    proxyClientConnect(&result2, clientHandle, &proxy, 1);

    EXPECT_EQ(result2->numErrors, 1);
    EXPECT_STREQ(result2->errors->code, ERROR_INVALID_PROXY_URL);
    proxyClientResultDelete(result2);

    const ProxyResult* result3 = nullptr;
    proxyClientDelete(&result3, clientHandle);
    proxyClientResultDelete(result3);
}

TEST_F(StreamrProxyClientTest, InvalidProxyEthereumAddress) {
    const ProxyResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    const ProxyResult* result2 = nullptr;
    Proxy proxy{
        .websocketUrl = validProxyUrl,
        .ethereumAddress = invalidEthereumAddress};
    proxyClientConnect(&result2, clientHandle, &proxy, 1);

    EXPECT_EQ(result2->numErrors, 1);
    EXPECT_STREQ(result2->errors->code, ERROR_INVALID_ETHEREUM_ADDRESS);
    proxyClientResultDelete(result2);

    const ProxyResult* result3 = nullptr;
    proxyClientDelete(&result3, clientHandle);
    proxyClientResultDelete(result3);
}

TEST_F(StreamrProxyClientTest, ProxyConnectionFailed) {
    const ProxyResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    proxyClientResultDelete(result);
    const ProxyResult* result2 = nullptr;
    Proxy proxy{
        .websocketUrl = nonExistentProxyUrl0,
        .ethereumAddress = validEthereumAddress};
    proxyClientConnect(&result2, clientHandle, &proxy, 1);

    SLogger::info("numErrors: " + std::to_string(result2->numErrors));
    SLogger::info("errors: " + std::string(result2->errors->message));
    SLogger::info("errors: " + std::string(result2->errors->code));

    EXPECT_EQ(result2->numErrors, 1);
    EXPECT_STREQ(result2->errors->code, ERROR_PROXY_CONNECTION_FAILED);
    proxyClientResultDelete(result2);

    const ProxyResult* result3 = nullptr;
    proxyClientDelete(&result3, clientHandle);
    proxyClientResultDelete(result3);
}

TEST_F(StreamrProxyClientTest, ThreeProxyConnectionsFailed) {
    const ProxyResult* result = nullptr;

    const char* ownEthereumAddress = goodEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    proxyClientResultDelete(result);

    // NOLINTNEXTLINE
    Proxy proxies[] = {
        {.websocketUrl = nonExistentProxyUrl0,
         .ethereumAddress = validEthereumAddress},
        {.websocketUrl = nonExistentProxyUrl1,
         .ethereumAddress = validEthereumAddress2},
        {.websocketUrl = nonExistentProxyUrl2,
         .ethereumAddress = validEthereumAddress3}};

    const ProxyResult* result2 = nullptr;
    auto numConnections =
        proxyClientConnect(&result2, clientHandle, proxies, 3);

    EXPECT_EQ(numConnections, 0);
    EXPECT_EQ(result2->numErrors, 3);
    EXPECT_EQ(
        std::string(result2->errors[0].proxy->websocketUrl),
        std::string(proxies[0].websocketUrl));
    EXPECT_EQ(
        std::string(result2->errors[1].proxy->websocketUrl),
        std::string(proxies[1].websocketUrl));
    EXPECT_EQ(
        std::string(result2->errors[2].proxy->websocketUrl),
        std::string(proxies[2].websocketUrl));

    for (uint64_t i = 0; i < result2->numErrors; ++i) {
        SLogger::info("errors: " + std::string(result2->errors[i].message));
        EXPECT_STREQ(result2->errors[i].code, ERROR_PROXY_CONNECTION_FAILED);
    }
    const ProxyResult* result3 = nullptr;
    proxyClientDelete(&result3, clientHandle);
    proxyClientResultDelete(result3);
}
