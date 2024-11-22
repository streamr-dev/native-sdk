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
SHARED_EXPORT static void __attribute__((constructor)) initialize(void);


/**
 * @brief Initialize the library. This function is called automatically when the library is loaded,
 * but can be called explicitly to force a re-initialization (e.g. after proxyClientCleanupLibrary() has been called).
 */

// NOLINTNEXTLINE
EXTERN_C SHARED_EXPORT void proxyClientInitLibrary(void);

/**
 * @brief Cleanup the library. This function MUST be called before the program exits.
 * Can be safely called multiple times. This is needed because the standard dynamic library
 * destructor (__attribute__((destructor))) is called after static variables have already been destroyed, 
 * which makes it impossible to clean up the other objects that depend on the static variables.
 */
// NOLINTNEXTLINE
EXTERN_C SHARED_EXPORT void proxyClientCleanupLibrary(void);
// NOLINTNEXTLINE
typedef struct Proxy {
    const char* websocketUrl;
    const char* ethereumAddress;
} Proxy;

// NOLINTNEXTLINE
typedef struct Error {
    const char* message;
    const char* code;
    const struct Proxy* proxy;
} Error;

// NOLINTNEXTLINE
typedef struct ProxyResult {
    Error* errors;
    uint64_t numErrors;
    Proxy* successful;
    uint64_t numSuccessful;
} ProxyResult;

/**
 * @brief Delete a ProxyResult. This method must be called after every call that
 * returns a ProxyResult.
 * @param proxyResult The ProxyResult to delete.
 */

EXTERN_C SHARED_EXPORT void proxyClientResultDelete(const ProxyResult* proxyResult);

/**
 * @brief Create a new proxy client.
 *
 * @param proxyResult Pointer in which ProxyResult will be stored. You MUST call
 * proxyClientResultDelete() on this after the call returns. The resulting ProxyResult
 * may only contain  "errors" - the "successful" and "numSuccessful" fields are
 * unused.
 * @param ownEthereumAddress The Ethereum address of the client in format
 * 0x1234567890123456789012345678901234567890.
 * @param streamPartId The stream part id in format
 * 0xa000000000000000000000000000000000000000#01.
 * @return The handle of the created client.
 */

EXTERN_C SHARED_EXPORT uint64_t proxyClientNew(
    const ProxyResult** proxyResult,
    const char* ownEthereumAddress,
    const char* streamPartId);

/**
 * @brief Delete a proxy client.
 *
 * @param proxyResult Pointer in which ProxyResult will be stored. You MUST call
 * proxyClientResultDelete() on this after the call returns. The resulting ProxyResult
 * may only contain  "errors" - the "successful" and "numSuccessful" fields are
 * unused.
 * @param clientHandle The client handle of the client to delete.
 */

EXTERN_C SHARED_EXPORT void proxyClientDelete(
    const ProxyResult** proxyResult, uint64_t clientHandle);

/**
 * @brief Connect a proxy client to a list of proxies.
 *
 * @param proxyResult Pointer in which ProxyResult will be stored. You MUST call
 * proxyClientResultDelete() on this after the call returns.
 * @param clientHandle The client handle of the client to connect.
 * @param proxies The array of proxies.
 * @param numProxies The number of proxies.
 * @return The number of proxies connected to.
 */

EXTERN_C SHARED_EXPORT uint64_t proxyClientConnect(
    const ProxyResult** proxyResult,
    uint64_t clientHandle,
    const Proxy* proxies,
    uint64_t numProxies);

/**
 * @brief Disconnect a proxy client from all proxies.
 *
 * @param proxyResult Pointer in which ProxyResult will be stored. You MUST call
 * proxyClientResultDelete() on this after the call returns. The resulting ProxyResult
 * may only contain  "errors" - the "successful" and "numSuccessful" fields are
 * unused.
 * @param clientHandle The client handle of the client to disconnect.
 
EXTERN_C SHARED_EXPORT void proxyClientDisconnect(
    const ProxyResult** proxyResult, uint64_t clientHandle);
*/

/**
 * @brief Publish a message to the stream.
 *
 * @param proxyResult Pointer in which ProxyResult will be stored. You MUST call
 * proxyClientResultDelete() on this after the call returns.
 * @param clientHandle The client handle of the client to publish.
 * @param content The content to publish.
 * @param contentLength The length of the content.
 * @param ethereumPrivateKey as hex without 0x prefix (64 characters) or NULL if
 * message should not be signed.
 * @return The number of proxies to which the message was published to.
 */

EXTERN_C SHARED_EXPORT uint64_t proxyClientPublish(
    const ProxyResult** proxyResult,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength,
    const char* ethereumPrivateKey);

#endif /* LIBSTREAMRPROXYCLIENT_H */
