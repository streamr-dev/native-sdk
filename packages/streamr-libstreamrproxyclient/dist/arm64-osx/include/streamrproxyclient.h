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

/**
 * @brief Create a new proxy client.
 *
 * @param errors The array of errors or NULL.
 * @param numErrors The number of errors.
 * @param ownEthereumAddress The Ethereum address of the client in format
 * 0x1234567890123456789012345678901234567890.
 * @param streamPartId The stream part id in format
 * 0xa000000000000000000000000000000000000000#01.
 * @return The handle of the created client.
 */

EXTERN_C SHARED_EXPORT uint64_t proxyClientNew(
    Error** errors,
    uint64_t* numErrors,
    const char* ownEthereumAddress,
    const char* streamPartId);

/**
 * @brief Delete a proxy client.
 *
 * @param errors The array of errors or NULL.
 * @param numErrors The number of errors.
 * @param clientHandle The client handle of the client to delete.
 */

EXTERN_C SHARED_EXPORT void proxyClientDelete(
    Error** errors, uint64_t* numErrors, uint64_t clientHandle);

/**
 * @brief Connect a proxy client to a list of proxies.
 *
 * @param errors The array of errors or NULL.
 * @param numErrors The number of errors.
 * @param clientHandle The client handle of the client to connect.
 * @param proxies The array of proxies.
 * @param numProxies The number of proxies.
 * @return The number of proxies connected to.
 */

EXTERN_C SHARED_EXPORT uint64_t proxyClientConnect(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const Proxy* proxies,
    uint64_t numProxies);

/**
 * @brief Disconnect a proxy client from all proxies.
 *
 * @param errors The array of errors or NULL.
 * @param numErrors The number of errors.
 * @param clientHandle The client handle of the client to disconnect.
 */

EXTERN_C SHARED_EXPORT void proxyClientDisconnect(
    Error** errors, uint64_t* numErrors, uint64_t clientHandle);

/**
 * @brief Publish a message to the stream.
 *
 * @param errors The array of errors or NULL.
 * @param numErrors The number of errors.
 * @param clientHandle The client handle of the client to publish.
 * @param content The content to publish.
 * @param contentLength The length of the content.
 * @param ethereumPrivateKey as hex without 0x prefix (64 characters) or NULL if message should not be signed.
 * @return The number of proxies to which the message was published to.
 */

EXTERN_C SHARED_EXPORT uint64_t proxyClientPublish(
    Error** errors,
    uint64_t* numErrors,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength,
    const char* ethereumPrivateKey);

#endif /* LIBSTREAMRPROXYCLIENT_H */
