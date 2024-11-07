#include <string>
#include <gtest/gtest.h>
#include "StreamrProxyClient.hpp"

using streamr::libstreamrproxyclient::StreamrProxyAddress;
using streamr::libstreamrproxyclient::StreamrProxyClient;

class PublishToTsServerCppTest : public ::testing::Test {
protected:
    static constexpr const char* ownEthereumAddress =
        "0x1234567890123456789012345678901234567890";

    static constexpr const char* tsEthereumAddress =
        "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    static constexpr const char* tsProxyUrl = "ws://127.0.0.1:44211";
    static constexpr const char* tsStreamPartId =
        "0xa000000000000000000000000000000000000000#01";

public:
    //~PublishToTsServerCppTest() override { proxyClientCleanupLibrary(); }
};

TEST_F(PublishToTsServerCppTest, ProxyPublish) noexcept(false) {
    // Create client
    StreamrProxyClient client(ownEthereumAddress, tsStreamPartId);

    // Connect to proxy
    std::vector<StreamrProxyAddress> proxies = {
        StreamrProxyAddress{tsProxyUrl, tsEthereumAddress}};

    auto connectResult = client.connect(proxies);
    EXPECT_EQ(connectResult.failed.size(), 0);
    EXPECT_GT(connectResult.numConnected, 1);

    // Publish message
    std::string message = "Hello from libstreamrproxyclient!";
    auto publishResult = client.publish(message, "");  

    // Verify publish results
    EXPECT_EQ(publishResult.failed.size(), 0);
    EXPECT_GT(publishResult.numConnected, 1);
}
