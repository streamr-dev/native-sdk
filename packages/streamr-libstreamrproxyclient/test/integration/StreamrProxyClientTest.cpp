#include <gtest/gtest.h>
#include "streamrproxyclient.h"
#include <iostream>

TEST(StreamrProxyClientTest, CanCallApi) {

    std::cout << "RPC returned: " << testRpc() << "\n";
}

TEST(StreamrProxyClientTest, CanCreateAndDeleteProxyClient) {
    // uint64_t clientHandle = proxyClientNew();
    // EXPECT_NE(clientHandle, 0);
    // proxyClientDelete(clientHandle);
}
