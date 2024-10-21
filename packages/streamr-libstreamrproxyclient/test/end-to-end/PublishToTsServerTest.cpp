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
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, tsStreamPartId);

    EXPECT_EQ(numErrors, 0);
    EXPECT_EQ(errors, nullptr);

    Proxy proxy{
        .websocketUrl = tsProxyUrl, .ethereumAddress = tsEthereumAddress};

    proxyClientConnect(&errors, &numErrors, clientHandle, &proxy, 1);

    EXPECT_EQ(numErrors, 0);
    EXPECT_EQ(errors, nullptr);

    std::string message = "Hello from libstreamrproxyclient!";
    proxyClientPublish(
        &errors,
        &numErrors,
        clientHandle,
        message.c_str(),
        message.length(),
        nullptr);

    EXPECT_EQ(numErrors, 0);
    EXPECT_EQ(errors, nullptr);

    proxyClientDelete(&errors, &numErrors, clientHandle);

    EXPECT_EQ(numErrors, 0);
    EXPECT_EQ(errors, nullptr);
}

TEST(PublishToTsServerTest, ProxyPublishMultipleMessages) {
    Error* errors = nullptr;
    uint64_t numErrors = 0;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, tsStreamPartId);

    Proxy proxy{
        .websocketUrl = tsProxyUrl, .ethereumAddress = tsEthereumAddress};

    proxyClientConnect(&errors, &numErrors, clientHandle, &proxy, 1);

    std::string message1 = "Hello from libstreamrproxyclient! m1";
    std::string message2 = "Hello from libstreamrproxyclient! m2";
    std::string message3 = "Hello from libstreamrproxyclient! m3";

    proxyClientPublish(
        &errors,
        &numErrors,
        clientHandle,
        message1.c_str(),
        message1.length(),
        nullptr);
    proxyClientPublish(
        &errors,
        &numErrors,
        clientHandle,
        message2.c_str(),
        message2.length(),
        nullptr);
    proxyClientPublish(
        &errors,
        &numErrors,
        clientHandle,
        message3.c_str(),
        message3.length(),
        nullptr);

    EXPECT_EQ(numErrors, 0);
    EXPECT_EQ(errors, nullptr);

    proxyClientDelete(&errors, &numErrors, clientHandle);

    EXPECT_EQ(numErrors, 0);
    EXPECT_EQ(errors, nullptr);
}
