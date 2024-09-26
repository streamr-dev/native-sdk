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
        &errors, &numErrors, clientHandle, message.c_str(), message.length());

    EXPECT_EQ(numErrors, 0);
    EXPECT_EQ(errors, nullptr);

    proxyClientDelete(&errors, &numErrors, clientHandle);
}
