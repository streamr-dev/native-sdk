#include <cassert>
#include <iostream>
#include <string>
#include "StreamrProxyClient.hpp"

int main() {
    // This is a widely-used test account
    const std::string ownEthereumAddress =
        "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5";
    const std::string ethereumPrivateKey =
        "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820";
    const std::string streamPartId =
        "0xd2078dc2d780029473a39ce873fc182587be69db/low-level-client#0";

    try {
        // Create client instance
        streamrproxyclient::StreamrProxyClient client(
            ownEthereumAddress, ethereumPrivateKey, streamPartId);

        // Connect to proxy
        auto connectResult = client.connect(
            {{.websocketUrl = "ws://95.216.15.80:44211",
              .ethereumAddress =
                  "0xd0d14b38d1f6b59d3772a63d84ece0a79e6e1c1f"}});
        assert(connectResult.errors.empty());

        // Publish message
        const std::string message = "Hello from libstreamrproxyclient C++!";

        auto publishResult = client.publish(message);
        assert(publishResult.errors.empty());

        std::cout << ownEthereumAddress << " published message \"" << message
                  << "\" to " << publishResult.successful.size()
                  << " proxies\n";

    } catch (const streamrproxyclient::StreamrProxyError& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}