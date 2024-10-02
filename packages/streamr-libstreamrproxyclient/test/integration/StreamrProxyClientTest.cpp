#include "streamrproxyclient.h"
#include <iostream>
#include <gtest/gtest.h>
#include "streamr-logger/SLogger.hpp"

using streamr::logger::SLogger;

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

TEST(StreamrProxyClientTest, CanCallApi) {
    std::cout << "RPC returned: " << testRpc() << "\n";
}

TEST(StreamrProxyClientTest, CanCreateAndDeleteProxyClient) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    EXPECT_EQ(numErrors, 0);
    EXPECT_EQ(errors, nullptr);
    EXPECT_NE(clientHandle, 0);

    proxyClientDelete(&errors, &numErrors, clientHandle);
    SLogger::info("proxyClientDelete called");
    EXPECT_EQ(numErrors, 0);
    EXPECT_EQ(errors, nullptr);
    SLogger::info("test finished");
}

TEST(StreamrProxyClientTest, InvalidEthereumAddress) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    const char* ownEthereumAddress = invalidEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    SLogger::info("numErrors: " + std::to_string(numErrors));
    SLogger::info("errors: " + std::string(errors->message));
    SLogger::info("errors: " + std::string(errors->code));

    EXPECT_EQ(numErrors, 1);
    EXPECT_STREQ(errors->code, ERROR_INVALID_ETHEREUM_ADDRESS);
}

TEST(StreamrProxyClientTest, InvalidStreamPartId) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = invalidStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    SLogger::info("numErrors: " + std::to_string(numErrors));
    SLogger::info("errors: " + std::string(errors->message));
    SLogger::info("errors: " + std::string(errors->code));

    EXPECT_EQ(numErrors, 1);
    EXPECT_STREQ(errors->code, ERROR_INVALID_STREAM_PART_ID);
}

TEST(StreamrProxyClientTest, ProxyClientNotFound) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    Proxy proxy{
        .websocketUrl = invalidProxyUrl,
        .ethereumAddress = validEthereumAddress};

    proxyClientConnect(&errors, &numErrors, invalidClientHandle, &proxy, 1);

    EXPECT_EQ(numErrors, 1);
    EXPECT_STREQ(errors->code, ERROR_PROXY_CLIENT_NOT_FOUND);
}

TEST(StreamrProxyClientTest, NoProxiesDefined) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    proxyClientConnect(&errors, &numErrors, clientHandle, nullptr, 0);

    EXPECT_EQ(numErrors, 1);
    EXPECT_STREQ(errors->code, ERROR_NO_PROXIES_DEFINED);

    proxyClientDelete(&errors, &numErrors, clientHandle);
}

TEST(StreamrProxyClientTest, InvalidProxyUrl) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    Proxy proxy{
        .websocketUrl = invalidProxyUrl,
        .ethereumAddress = validEthereumAddress};
    proxyClientConnect(&errors, &numErrors, clientHandle, &proxy, 1);

    EXPECT_EQ(numErrors, 1);
    EXPECT_STREQ(errors->code, ERROR_INVALID_PROXY_URL);
    proxyClientDelete(&errors, &numErrors, clientHandle);
}

TEST(StreamrProxyClientTest, InvalidProxyEthereumAddress) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    Proxy proxy{
        .websocketUrl = validProxyUrl,
        .ethereumAddress = invalidEthereumAddress};
    proxyClientConnect(&errors, &numErrors, clientHandle, &proxy, 1);

    EXPECT_EQ(numErrors, 1);
    EXPECT_STREQ(errors->code, ERROR_INVALID_ETHEREUM_ADDRESS);

    proxyClientDelete(&errors, &numErrors, clientHandle);
}

TEST(StreamrProxyClientTest, ProxyConnectionFailed) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    Proxy proxy{
        .websocketUrl = nonExistentProxyUrl0,
        .ethereumAddress = validEthereumAddress};
    proxyClientConnect(&errors, &numErrors, clientHandle, &proxy, 1);

    SLogger::info("numErrors: " + std::to_string(numErrors));
    SLogger::info("errors: " + std::string(errors->message));
    SLogger::info("errors: " + std::string(errors->code));

    EXPECT_EQ(numErrors, 1);
    EXPECT_STREQ(errors->code, ERROR_PROXY_CONNECTION_FAILED);

    proxyClientDelete(&errors, &numErrors, clientHandle);
}

TEST(StreamrProxyClientTest, ThreeProxyConnectionsFailed) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    const char* ownEthereumAddress = goodEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    // NOLINTNEXTLINE
    Proxy proxies[] = {
        {.websocketUrl = nonExistentProxyUrl0,
         .ethereumAddress = validEthereumAddress},
        {.websocketUrl = nonExistentProxyUrl1,
         .ethereumAddress = validEthereumAddress2},
        {.websocketUrl = nonExistentProxyUrl2,
         .ethereumAddress = validEthereumAddress3}};

    auto numConnections =
        proxyClientConnect(&errors, &numErrors, clientHandle, proxies, 3);

    EXPECT_EQ(numConnections, 0);
    EXPECT_EQ(numErrors, 3);

    for (uint64_t i = 0; i < numErrors; ++i) {
        SLogger::info("errors: " + std::string(errors[i].message));
        EXPECT_STREQ(errors[i].code, ERROR_PROXY_CONNECTION_FAILED);
    }
    proxyClientDelete(&errors, &numErrors, clientHandle);
}
