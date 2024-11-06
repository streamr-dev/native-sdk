#include "streamrproxyclient.h"
#include <folly/Singleton.h>
#include "LibProxyClientApi.hpp"
#include <iostream>
#include "streamr-logger/SLogger.hpp"

using streamr::libstreamrproxyclient::LibProxyClientApi;
using streamr::logger::SLogger;
const char* testRpc() {
    return "Hajotkaa siihen";
}

static void initFolly() { // NOLINT
    folly::SingletonVault::singleton()->registrationComplete();
}

static LibProxyClientApi* libProxyClientApi = nullptr; // NOLINT
static bool cleanupCalled = false; // NOLINT

static void initialize() { // NOLINT
    std::cout << "initialize()" << "\n";
    libProxyClientApi = new LibProxyClientApi();
}

void proxyClientCleanupLibrary() { // NOLINT
    if (cleanupCalled) {
        return;
    }
    SLogger::info("cleanup()");
    //delete libProxyClientApi;
    SLogger::info("cleanup() done");

    cleanupCalled = true;
}

static LibProxyClientApi& getProxyClientApi() { // NOLINT
    return *libProxyClientApi;
}

void proxyClientResultDelete(const ProxyResult* proxyResult) {
    getProxyClientApi().proxyClientResultDelete(proxyResult);
}

uint64_t proxyClientNew(
    const ProxyResult** proxyResult,
    const char* ownEthereumAddress,
    const char* streamPartId) {
    initFolly(); // this can be safely called multiple times
    return getProxyClientApi().proxyClientNew(
        proxyResult, ownEthereumAddress, streamPartId);
}

void proxyClientDelete(
    const ProxyResult** proxyResult, uint64_t clientHandle) {
    getProxyClientApi().proxyClientDelete(proxyResult, clientHandle);
}

uint64_t proxyClientConnect(
    const ProxyResult** proxyResult,
    uint64_t clientHandle,
    const Proxy* proxies,
    uint64_t numProxies) {
    return getProxyClientApi().proxyClientConnect(
        proxyResult, clientHandle, proxies, numProxies);
}

uint64_t proxyClientPublish(
    const ProxyResult** proxyResult,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength,
    const char* ethereumPrivateKey) {
    return getProxyClientApi().proxyClientPublish(
        proxyResult,
        clientHandle,
        content,
        contentLength,
        ethereumPrivateKey);
}
