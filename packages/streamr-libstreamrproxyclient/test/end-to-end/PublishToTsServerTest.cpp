#include <string>
#include <gtest/gtest.h>
#include "streamrproxyclient.h"

static constexpr const char* ownEthereumAddress =
    "0x1234567890123456789012345678901234567890";

static constexpr const char* tsEthereumAddress =
    "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
static constexpr const char* tsProxyUrl = "ws://127.0.0.1:44211";
static constexpr const char* tsStreamPartId =
    "0xa000000000000000000000000000000000000000#01";

TEST(PublishToTsServerTest, ProxyPublish) {
    const ProxyResult* result = nullptr;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, tsStreamPartId);

    EXPECT_EQ(result->numErrors, 0);
    proxyClientResultDelete(result);

    Proxy proxy{
        .websocketUrl = tsProxyUrl, .ethereumAddress = tsEthereumAddress};

    const ProxyResult* result2 = nullptr;
    proxyClientConnect(&result2, clientHandle, &proxy, 1);

    EXPECT_EQ(result2->numErrors, 0);
    proxyClientResultDelete(result2);

    std::string message = "Hello from libstreamrproxyclient!";

    const ProxyResult* result3 = nullptr;
    proxyClientPublish(
        &result3, clientHandle, message.c_str(), message.length(), nullptr);

    EXPECT_EQ(result3->numErrors, 0);
    proxyClientResultDelete(result3);

    const ProxyResult* result4 = nullptr;
    proxyClientDelete(&result4, clientHandle);
    proxyClientResultDelete(result4);
}

TEST(PublishToTsServerTest, ProxyPublishMultipleMessages) {
    const ProxyResult* result = nullptr;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, tsStreamPartId);

    EXPECT_EQ(result->numErrors, 0);
    proxyClientResultDelete(result);

    Proxy proxy{
        .websocketUrl = tsProxyUrl, .ethereumAddress = tsEthereumAddress};

    const ProxyResult* result2 = nullptr;
    proxyClientConnect(&result2, clientHandle, &proxy, 1);
    proxyClientResultDelete(result2);
    std::string message1 = "Hello from libstreamrproxyclient! m1";
    std::string message2 = "Hello from libstreamrproxyclient! m2";
    std::string message3 = "Hello from libstreamrproxyclient! m3";

    const ProxyResult* result3 = nullptr;
    proxyClientPublish(
        &result3, clientHandle, message1.c_str(), message1.length(), nullptr);
    proxyClientResultDelete(result3);

    const ProxyResult* result4 = nullptr;
    proxyClientPublish(
        &result4, clientHandle, message2.c_str(), message2.length(), nullptr);
    proxyClientResultDelete(result4);

    const ProxyResult* result5 = nullptr;
    proxyClientPublish(
        &result5, clientHandle, message3.c_str(), message3.length(), nullptr);
    proxyClientResultDelete(result5);

    const ProxyResult* result6 = nullptr;
    proxyClientDelete(&result6, clientHandle);

    proxyClientResultDelete(result6);
}
