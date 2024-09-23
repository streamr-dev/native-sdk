#include "LibProxyClientApi.hpp"
#include "streamrproxyclient.h"

using streamr::libstreamrproxyclient::LibProxyClientApi;

const char* testRpc() {
    return "Hajotkaa siihen";
}

static LibProxyClientApi proxyClientApi; //NOLINT

uint64_t proxyClientNew(Error* error,
    uint64_t* numErrors,
    const char* ownEthereumAddress,
    const char* streamPartId) {
    return proxyClientApi.proxyClientNew(error, numErrors, ownEthereumAddress, streamPartId);
}

void proxyClientDelete(Error* error, uint64_t* numErrors, uint64_t clientHandle) {
    proxyClientApi.proxyClientDelete(error, numErrors, clientHandle);
}
