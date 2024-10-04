#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include "streamrproxyclient.h"

int main() {
    static constexpr const char* ownEthereumAddress =
        "0xbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

    static constexpr const char* tsEthereumAddress =
        "0xaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    static constexpr const char* tsProxyUrl = "ws://127.0.0.1:44211";
    static constexpr const char* tsStreamPartId =
        "0xa000000000000000000000000000000000000000#01";
    
    static constexpr const char* productionProxyUrl =
        "wss://95.216.15.80:32200";
    static constexpr const char* productionStreamPartId =
        "0xa000000000000000000000000000000000000000#01";
    static constexpr const char* productionEthereumAddress =
        "0x5bb9deae7df3d9d7f1ddb2f73d29134127ef4b1b";

    Error* errors = nullptr;
    uint64_t numErrors = 0;

    uint64_t clientHandle =
        proxyClientNew(&errors, &numErrors, ownEthereumAddress, tsStreamPartId);

    assert(numErrors == 0);
    assert(errors == nullptr);

    Proxy proxy{
        .websocketUrl = tsProxyUrl, .ethereumAddress = tsEthereumAddress};

    proxyClientConnect(&errors, &numErrors, clientHandle, &proxy, 1);

    assert(numErrors == 0);
    assert(errors == nullptr);

    std::string message = "Hello from libstreamrproxyclient!";

    while (true) {
        std::cout << "Publishing message" << "\n";
        proxyClientPublish(
            &errors,
            &numErrors,
            clientHandle,
            message.c_str(),
            message.length());

        assert(numErrors == 0);
        assert(errors == nullptr);
        std::cout << "Published message" << "\n";
        std::cout << "Sleeping for 15 seconds" << "\n";
        std::this_thread::sleep_for(std::chrono::seconds(15)); // NOLINT
        std::cout << "Sleeping done" << "\n";
    }

    proxyClientDelete(&errors, &numErrors, clientHandle);

    assert(numErrors == 0);
    assert(errors == nullptr);

    return 0;
}
