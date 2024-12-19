#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include "StreamrProxyClient.hpp"

using streamr::libstreamrproxyclient::StreamrProxyClient;
using streamr::libstreamrproxyclient::StreamrProxyAddress;
using streamr::libstreamrproxyclient::Err;

std::string generateRandomEthereumAddress() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15); // NOLINT

    std::stringstream ss;
    ss << "0x";
    for (int i = 0; i < 40; ++i) { // NOLINT
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <proxy_server_url>"
                  << " <proxy_server_ethereum_address>"
                  << " <stream_part_id>"
                  << "\n";
        return 1;
    }

    const std::string proxyUrl = argv[1];
    const std::string proxyServerEthereumAddress = argv[2];
    const std::string streamPartId = argv[3];

    // This is a widely-used test account
    const std::string ownEthereumAddress = "0xa5374e3c19f15e1847881979dd0c6c9ffe846bd5";
    const std::string ethereumPrivateKey = "23bead9b499af21c4c16e4511b3b6b08c3e22e76e0591f5ab5ba8d4c3a5b1820";

    try {
        // Create client using C++ API
        StreamrProxyClient client(ownEthereumAddress, streamPartId);

        // Setup proxy connection
        std::vector<StreamrProxyAddress> proxies = {
            {proxyUrl, proxyServerEthereumAddress}
        };
        
        auto connectResult = client.connect(proxies);
        std::cout << "Connected to " << connectResult.numConnected << " proxies\n";

        std::string message = "Hello from libstreamrproxyclient!";

        while (true) {
            std::cout << "Publishing message\n";
            auto publishResult = client.publish(message, ethereumPrivateKey);

            std::cout << ownEthereumAddress << " published message "
                     << "\"" << message << "\""
                     << " to " << publishResult.numConnected << " proxies\n";
            
            std::cout << "Sleeping for 15 seconds\n";
            std::this_thread::sleep_for(std::chrono::seconds(15));
            std::cout << "Sleeping done\n";
        }

    } catch (const Err& e) {
        std::cerr << "Error: " << e.message << "\n";
        return 1;
    }

    return 0;
}