#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include "streamrproxyclient.h"

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
    const char* proxyUrl = argv[1];
    const char* proxyServerEthereumAddress = argv[2];
    const char* streamPartId = argv[3];

    const std::string ownEthereumAddressString =
        generateRandomEthereumAddress();
    const char* ownEthereumAddress = ownEthereumAddressString.c_str();

    Error* errors = nullptr;
    uint64_t numErrors = 0;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, streamPartId);

    assert(numErrors == 0);
    assert(errors == nullptr);

    Proxy proxy{
        .websocketUrl = proxyUrl,
        .ethereumAddress = proxyServerEthereumAddress};

    proxyClientConnect(&errors, &numErrors, clientHandle, &proxy, 1);

    assert(numErrors == 0);
    assert(errors == nullptr);

    std::string message = "Hello from libstreamrproxyclient!";

    while (true) {
        std::cout << "Publishing message"
                  << "\n";
        uint64_t numProxiesPublishedTo = proxyClientPublish(
            &errors,
            &numErrors,
            clientHandle,
            message.c_str(),
            message.length());

        assert(numErrors == 0);
        assert(errors == nullptr);
        
        std::cout << ownEthereumAddress << " published message "
                  << "\"" << message << "\""
                  << " to " << numProxiesPublishedTo << " proxies"
                  << "\n";
        std::cout << "Sleeping for 15 seconds"
                  << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(15)); // NOLINT
        std::cout << "Sleeping done"
                  << "\n";
    }

    proxyClientDelete(&errors, &numErrors, clientHandle);

    assert(numErrors == 0);
    assert(errors == nullptr);

    return 0;
}
