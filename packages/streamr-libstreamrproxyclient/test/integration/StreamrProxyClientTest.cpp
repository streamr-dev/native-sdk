#include "streamrproxyclient.h"
#include <iostream>
#include <string>
#include <gtest/gtest.h>
import streamr.logger.SLogger;

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
    ~StreamrProxyClientTest() override { streamrCleanupLibrary(); }
};

TEST_F(StreamrProxyClientTest, CanCallApi) {
    std::cout << "RPC returned: " << testRpc() << "\n";
}

TEST_F(StreamrProxyClientTest, CanCreateAndDeleteProxyClient) {
    const StreamrResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    EXPECT_EQ(result->numErrors, 0);
    EXPECT_NE(clientHandle, 0);
    streamrResultDelete(result);

    const StreamrResult* result2 = nullptr;
    proxyClientDelete(&result2, clientHandle);
    SLogger::info("proxyClientDelete called");
    EXPECT_EQ(result2->numErrors, 0);
    streamrResultDelete(result2);

    SLogger::info("test finished");
}

// Regression test for use-after-cleanup: the fixture destructor calls
// streamrCleanupLibrary() after every test, so any test that runs
// after another one in the same process starts with the library torn
// down. API entry points must re-initialize the library on demand
// instead of dereferencing the destroyed instance (this crashed with
// SIGSEGV before the fix; CI masked it by running every test in its
// own process).
TEST_F(StreamrProxyClientTest, ApiIsUsableAfterCleanupLibrary) {
    // Implicit re-initialization: call the API right after a cleanup.
    streamrCleanupLibrary();

    const StreamrResult* result = nullptr;
    uint64_t clientHandle =
        proxyClientNew(&result, validEthereumAddress, validStreamPartId);
    EXPECT_EQ(result->numErrors, 0);
    EXPECT_NE(clientHandle, 0);
    streamrResultDelete(result);

    const StreamrResult* result2 = nullptr;
    proxyClientDelete(&result2, clientHandle);
    EXPECT_EQ(result2->numErrors, 0);
    streamrResultDelete(result2);

    // Explicit re-initialization (the path documented in
    // streamrcommon.h): cleanup, init, use.
    streamrCleanupLibrary();
    streamrInitLibrary();

    const StreamrResult* result3 = nullptr;
    uint64_t clientHandle2 =
        proxyClientNew(&result3, validEthereumAddress, validStreamPartId);
    EXPECT_EQ(result3->numErrors, 0);
    EXPECT_NE(clientHandle2, 0);
    streamrResultDelete(result3);

    const StreamrResult* result4 = nullptr;
    proxyClientDelete(&result4, clientHandle2);
    EXPECT_EQ(result4->numErrors, 0);
    streamrResultDelete(result4);
}

TEST_F(StreamrProxyClientTest, InvalidEthereumAddress) {
    const StreamrResult* result = nullptr;

    const char* ownEthereumAddress = invalidEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    SLogger::info("numErrors: " + std::to_string(result->numErrors));
    SLogger::info("errors: " + std::string(result->errors->message));
    SLogger::info("errors: " + std::string(result->errors->code));

    EXPECT_EQ(result->numErrors, 1);
    EXPECT_STREQ(result->errors->code, ERROR_INVALID_ETHEREUM_ADDRESS);
    streamrResultDelete(result);
}

TEST_F(StreamrProxyClientTest, InvalidStreamPartId) {
    const StreamrResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = invalidStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    SLogger::info("numErrors: " + std::to_string(result->numErrors));
    SLogger::info("errors: " + std::string(result->errors->message));
    SLogger::info("errors: " + std::string(result->errors->code));

    EXPECT_EQ(result->numErrors, 1);
    EXPECT_STREQ(result->errors->code, ERROR_INVALID_STREAM_PART_ID);
    streamrResultDelete(result);
}

TEST_F(StreamrProxyClientTest, ProxyClientNotFound) {
    const StreamrResult* result = nullptr;

    StreamrPeer proxy{
        .websocketUrl = invalidProxyUrl,
        .ethereumAddress = validEthereumAddress};

    proxyClientConnect(&result, invalidClientHandle, &proxy, 1);

    EXPECT_EQ(result->numErrors, 1);
    EXPECT_STREQ(result->errors->code, ERROR_PROXY_CLIENT_NOT_FOUND);
    streamrResultDelete(result);
}

TEST_F(StreamrProxyClientTest, NoProxiesDefined) {
    const StreamrResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    proxyClientConnect(&result, clientHandle, nullptr, 0);

    EXPECT_EQ(result->numErrors, 1);
    EXPECT_STREQ(result->errors->code, ERROR_NO_PROXIES_DEFINED);
    streamrResultDelete(result);
}

TEST_F(StreamrProxyClientTest, InvalidProxyUrl) {
    const StreamrResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    streamrResultDelete(result);

    const StreamrResult* result2 = nullptr;
    StreamrPeer proxy{
        .websocketUrl = invalidProxyUrl,
        .ethereumAddress = validEthereumAddress};
    proxyClientConnect(&result2, clientHandle, &proxy, 1);

    EXPECT_EQ(result2->numErrors, 1);
    EXPECT_STREQ(result2->errors->code, ERROR_INVALID_PROXY_URL);
    streamrResultDelete(result2);

    const StreamrResult* result3 = nullptr;
    proxyClientDelete(&result3, clientHandle);
    streamrResultDelete(result3);
}

TEST_F(StreamrProxyClientTest, InvalidProxyEthereumAddress) {
    const StreamrResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    const StreamrResult* result2 = nullptr;
    StreamrPeer proxy{
        .websocketUrl = validProxyUrl,
        .ethereumAddress = invalidEthereumAddress};
    proxyClientConnect(&result2, clientHandle, &proxy, 1);

    EXPECT_EQ(result2->numErrors, 1);
    EXPECT_STREQ(result2->errors->code, ERROR_INVALID_ETHEREUM_ADDRESS);
    streamrResultDelete(result2);

    const StreamrResult* result3 = nullptr;
    proxyClientDelete(&result3, clientHandle);
    streamrResultDelete(result3);
}

TEST_F(StreamrProxyClientTest, ProxyConnectionFailed) {
    const StreamrResult* result = nullptr;

    const char* ownEthereumAddress = validEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    streamrResultDelete(result);
    const StreamrResult* result2 = nullptr;
    StreamrPeer proxy{
        .websocketUrl = nonExistentProxyUrl0,
        .ethereumAddress = validEthereumAddress};
    proxyClientConnect(&result2, clientHandle, &proxy, 1);

    SLogger::info("numErrors: " + std::to_string(result2->numErrors));
    SLogger::info("errors: " + std::string(result2->errors->message));
    SLogger::info("errors: " + std::string(result2->errors->code));

    EXPECT_EQ(result2->numErrors, 1);
    EXPECT_STREQ(result2->errors->code, ERROR_PROXY_CONNECTION_FAILED);
    streamrResultDelete(result2);

    const StreamrResult* result3 = nullptr;
    proxyClientDelete(&result3, clientHandle);
    streamrResultDelete(result3);
}

TEST_F(StreamrProxyClientTest, ThreeProxyConnectionsFailed) {
    const StreamrResult* result = nullptr;

    const char* ownEthereumAddress = goodEthereumAddress;
    const char* streamPartId = validStreamPartId;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, streamPartId);

    streamrResultDelete(result);

    // NOLINTNEXTLINE
    StreamrPeer proxies[] = {
        {.websocketUrl = nonExistentProxyUrl0,
         .ethereumAddress = validEthereumAddress},
        {.websocketUrl = nonExistentProxyUrl1,
         .ethereumAddress = validEthereumAddress2},
        {.websocketUrl = nonExistentProxyUrl2,
         .ethereumAddress = validEthereumAddress3}};

    const StreamrResult* result2 = nullptr;
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
    const StreamrResult* result3 = nullptr;
    proxyClientDelete(&result3, clientHandle);
    streamrResultDelete(result3);
}

// The pre-3.0 proxy-prefixed names must stay source- and ABI-compatible
// for one release (phase D3a): this test compiles against the deprecated
// aliases and calls the deprecated symbols on purpose.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
TEST(StreamrProxyClientDeprecatedAliasTest, Pre30NamesStillWork) {
    proxyClientInitLibrary();
    const ProxyResult* result = nullptr;
    Proxy proxy{
        .websocketUrl = "ws://127.0.0.1:44211",
        .ethereumAddress = "0x1234567890123456789012345678901234567890"};
    (void)proxy;
    const uint64_t handle = proxyClientNew(
        &result,
        "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5",
        "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->numErrors, 0);
    proxyClientResultDelete(result);
    result = nullptr;
    proxyClientDelete(&result, handle);
    ASSERT_NE(result, nullptr);
    // The Error alias and its pre-3.0 `proxy` member spelling stay usable.
    if (result->numErrors > 0) {
        const Error* firstError = &result->errors[0];
        (void)firstError->proxy;
    }
    proxyClientResultDelete(result);
}
#pragma clang diagnostic pop
