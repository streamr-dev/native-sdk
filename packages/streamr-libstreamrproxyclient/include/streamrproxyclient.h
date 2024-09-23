#ifndef LIBSTREAMRPROXYCLIENT_HPP
#define LIBSTREAMRPROXYCLIENT_HPP

#include <stdint.h> // NOLINT

#if defined(__clang__)
#define SHARED_EXPORT __attribute__((visibility("default")))
#define SHARED_LOCAL __attribute__((visibility("hidden")))
#endif

#if defined(__swift__)
#define EXTERN_C
#else
#define EXTERN_C extern "C"
#endif

EXTERN_C const char* SHARED_EXPORT testRpc();

// this file should be in C, not C++
EXTERN_C struct SHARED_EXPORT Proxy {
    const char* websocketUrl;
    const char* proxyEthereumAddress;
};

EXTERN_C struct SHARED_EXPORT Error {
    const char* message;
    const char* code;
};

EXTERN_C uint64_t SHARED_EXPORT proxyClientNew(
    Error* error,
    uint64_t* numErrors,
    const char* ownEthereumAddress,
    const char* streamPartId);

EXTERN_C void SHARED_EXPORT
proxyClientDelete(Error* error, uint64_t* numErrors, uint64_t clientHandle);

EXTERN_C void SHARED_EXPORT proxyClientConnect(
    Error* error,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const Proxy* proxies,
    size_t numProxies);

EXTERN_C void SHARED_EXPORT
proxyClientDisconnect(Error* error, uint64_t* numErrors, uint64_t clientHandle);

EXTERN_C void SHARED_EXPORT proxyClientPublish(
    Error* error,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength);

#endif // LIBSTREAMRPROXYCLIENT_HPP