#include "streamrproxyclient.h"
#include "LibProxyClientApi.hpp"

using streamr::libstreamrproxyclient::LibProxyClientApi;

const char* testRpc() {
    return "Hajotkaa siihen";
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
    size_t numProxies) {
    return getProxyClientApi().proxyClientConnect(
        errors, numErrors, clientHandle, proxies, numProxies);
}

void proxyClientPublish(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength) {
    getProxyClientApi().proxyClientPublish(
        errors, numErrors, clientHandle, content, contentLength);
}
