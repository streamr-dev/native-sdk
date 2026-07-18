#include <string>
#include <gtest/gtest.h>
#include "streamrproxyclient.h"

class PublishToTsServerTest : public ::testing::Test {
protected:
    static constexpr const char* ownEthereumAddress =
        "0x1234567890123456789012345678901234567890";

    static constexpr const char* tsEthereumAddress =
        "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    static constexpr const char* tsProxyUrl = "ws://127.0.0.1:44211";
    static constexpr const char* tsStreamPartId =
        "0xa000000000000000000000000000000000000000#01";

public:
    ~PublishToTsServerTest() override { streamrCleanupLibrary(); }
};

TEST_F(PublishToTsServerTest, ProxyPublish) {
    const StreamrResult* result = nullptr;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, tsStreamPartId);

    EXPECT_EQ(result->numErrors, 0);
    streamrResultDelete(result);

    StreamrPeer proxy{
        .websocketUrl = tsProxyUrl, .ethereumAddress = tsEthereumAddress};

    const StreamrResult* result2 = nullptr;
    proxyClientConnect(&result2, clientHandle, &proxy, 1);

    EXPECT_EQ(result2->numErrors, 0);
    streamrResultDelete(result2);

    std::string message = "Hello from libstreamrproxyclient!";

    const StreamrResult* result3 = nullptr;
    proxyClientPublish(
        &result3, clientHandle, message.c_str(), message.length(), nullptr);

    EXPECT_EQ(result3->numErrors, 0);
    streamrResultDelete(result3);

    const StreamrResult* result4 = nullptr;
    proxyClientDelete(&result4, clientHandle);
    streamrResultDelete(result4);
}

TEST_F(PublishToTsServerTest, ProxyPublishMultipleMessages) {
    const StreamrResult* result = nullptr;

    uint64_t clientHandle =
        proxyClientNew(&result, ownEthereumAddress, tsStreamPartId);

    EXPECT_EQ(result->numErrors, 0);
    streamrResultDelete(result);

    StreamrPeer proxy{
        .websocketUrl = tsProxyUrl, .ethereumAddress = tsEthereumAddress};

    const StreamrResult* result2 = nullptr;
    proxyClientConnect(&result2, clientHandle, &proxy, 1);
    streamrResultDelete(result2);
    std::string message1 = "Hello from libstreamrproxyclient! m1";
    std::string message2 = "Hello from libstreamrproxyclient! m2";
    std::string message3 = "Hello from libstreamrproxyclient! m3";

    const StreamrResult* result3 = nullptr;
    proxyClientPublish(
        &result3, clientHandle, message1.c_str(), message1.length(), nullptr);
    streamrResultDelete(result3);

    const StreamrResult* result4 = nullptr;
    proxyClientPublish(
        &result4, clientHandle, message2.c_str(), message2.length(), nullptr);
    streamrResultDelete(result4);

    const StreamrResult* result5 = nullptr;
    proxyClientPublish(
        &result5, clientHandle, message3.c_str(), message3.length(), nullptr);
    streamrResultDelete(result5);

    const StreamrResult* result6 = nullptr;
    proxyClientDelete(&result6, clientHandle);

    streamrResultDelete(result6);
}
