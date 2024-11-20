#include "streamrproxyclient.h"
#include <folly/Singleton.h>
#include "LibProxyClientApi.hpp"

using streamr::libstreamrproxyclient::LibProxyClientApi;

const char* testRpc() {
    return "Hajotkaa siihen";
}

static void initFolly() { // NOLINT

    folly::SingletonVault::singleton()->registrationComplete();
}

void initialize() {
    // getFollyInit();
}

static LibProxyClientApi& getProxyClientApi() { // NOLINT
    static LibProxyClientApi proxyClientApi; // NOLINT
    return proxyClientApi;
}

uint64_t proxyClientNew(
    Error** errors,
    uint64_t* numErrors,
    const char* ownEthereumAddress,
    const char* streamPartId) {
    initFolly(); // this can be safely called multiple times
    return getProxyClientApi().proxyClientNew(
        errors, numErrors, ownEthereumAddress, streamPartId);
}

void proxyClientDelete(
    Error** errors, uint64_t* numErrors, uint64_t clientHandle) {
    getProxyClientApi().proxyClientDelete(errors, numErrors, clientHandle);
}

uint64_t proxyClientConnect(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const Proxy* proxies,
    uint64_t numProxies) {
    return getProxyClientApi().proxyClientConnect(
        errors, numErrors, clientHandle, proxies, numProxies);
}

uint64_t proxyClientPublish(
    Error** errors,
    uint64_t* numErrors, // NOLINT
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength,
    const char* ethereumPrivateKey) {
    return getProxyClientApi().proxyClientPublish(
        errors,
        numErrors,
        clientHandle,
        content,
        contentLength,
        ethereumPrivateKey);
}
