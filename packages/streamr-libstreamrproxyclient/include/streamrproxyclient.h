#ifndef LIBSTREAMRPROXYCLIENT_HPP
#define LIBSTREAMRPROXYCLIENT_HPP

#include <stdint.h> // NOLINT

#if defined(__clang__) || defined(__GNUC__)
#define SHARED_EXPORT __attribute__((visibility("default")))
#define SHARED_LOCAL __attribute__((visibility("hidden")))
#endif

#if defined(__swift__)
#define EXTERN_C
#else
#define EXTERN_C extern "C"
#endif

#define ERROR_INVALID_ETHEREUM_ADDRESS "INVALID_ETHEREUM_ADDRESS"
#define ERROR_INVALID_STREAM_PART_ID "INVALID_STREAM_PART_ID"
#define ERROR_PROXY_CLIENT_NOT_FOUND "PROXY_CLIENT_NOT_FOUND"
#define ERROR_INVALID_PROXY_URL "INVALID_PROXY_URL"
#define ERROR_NO_PROXIES_DEFINED "NO_PROXIES_DEFINED"
#define ERROR_PROXY_CONNECTION_FAILED "PROXY_CONNECTION_FAILED"
#define ERROR_PROXY_BROADCAST_FAILED "PROXY_BROADCAST_FAILED"
EXTERN_C const char* SHARED_EXPORT testRpc();

// this file should be in C, not C++
EXTERN_C struct SHARED_EXPORT Proxy {
    const char* websocketUrl;
    const char* ethereumAddress;
};

EXTERN_C struct SHARED_EXPORT Error {
    const char* message;
    const char* code;
};

EXTERN_C uint64_t SHARED_EXPORT proxyClientNew(
    Error** error,
    uint64_t* numErrors,
    const char* ownEthereumAddress,
    const char* streamPartId);

EXTERN_C void SHARED_EXPORT
proxyClientDelete(Error** errors, uint64_t* numErrors, uint64_t clientHandle);

EXTERN_C uint64_t SHARED_EXPORT proxyClientConnect(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const Proxy* proxies,
    uint64_t numProxies);

EXTERN_C void SHARED_EXPORT proxyClientDisconnect(
    Error** errors, uint64_t* numErrors, uint64_t clientHandle);

EXTERN_C void SHARED_EXPORT proxyClientPublish(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength);

#endif // LIBSTREAMRPROXYCLIENT_HPP