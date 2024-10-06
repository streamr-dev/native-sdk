#ifndef LIBSTREAMRPROXYCLIENT_H
#define LIBSTREAMRPROXYCLIENT_H

// NOLINTNEXTLINE
#include <stdint.h>

#if defined(__clang__) || defined(__GNUC__)
#define SHARED_EXPORT __attribute__((visibility("default")))
#define SHARED_LOCAL __attribute__((visibility("hidden")))
#else
#define SHARED_EXPORT
#define SHARED_LOCAL
#endif

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif

#define ERROR_INVALID_ETHEREUM_ADDRESS "INVALID_ETHEREUM_ADDRESS"
#define ERROR_INVALID_STREAM_PART_ID "INVALID_STREAM_PART_ID"
#define ERROR_PROXY_CLIENT_NOT_FOUND "PROXY_CLIENT_NOT_FOUND"
#define ERROR_INVALID_PROXY_URL "INVALID_PROXY_URL"
#define ERROR_NO_PROXIES_DEFINED "NO_PROXIES_DEFINED"
#define ERROR_PROXY_CONNECTION_FAILED "PROXY_CONNECTION_FAILED"
#define ERROR_PROXY_BROADCAST_FAILED "PROXY_BROADCAST_FAILED"

EXTERN_C SHARED_EXPORT const char* testRpc(void);

// NOLINTNEXTLINE
static void __attribute__((constructor)) initialize(void);

// NOLINTNEXTLINE
typedef struct Proxy {
    const char* websocketUrl;
    const char* ethereumAddress;
} Proxy;

// NOLINTNEXTLINE
typedef struct Error {
    const char* message;
    const char* code;
} Error;

EXTERN_C SHARED_EXPORT uint64_t proxyClientNew(
    Error** error,
    uint64_t* numErrors,
    const char* ownEthereumAddress,
    const char* streamPartId);

EXTERN_C SHARED_EXPORT void proxyClientDelete(
    Error** errors, uint64_t* numErrors, uint64_t clientHandle);

EXTERN_C SHARED_EXPORT uint64_t proxyClientConnect(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const Proxy* proxies,
    uint64_t numProxies);

EXTERN_C SHARED_EXPORT void proxyClientDisconnect(
    Error** errors, uint64_t* numErrors, uint64_t clientHandle);

EXTERN_C SHARED_EXPORT uint64_t proxyClientPublish(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength);

#endif /* LIBSTREAMRPROXYCLIENT_H */
