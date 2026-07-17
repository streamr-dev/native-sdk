#ifndef LIBSTREAMRPROXYCLIENT_H
#define LIBSTREAMRPROXYCLIENT_H

// The proxy-client C API. The shared infrastructure (StreamrPeer,
// StreamrError, StreamrResult, streamrResultDelete, streamrInitLibrary,
// streamrCleanupLibrary) lives in streamrcommon.h and is shared with the
// full-node API (streamrnode.h); the pre-3.0 proxy-prefixed names below
// are deprecated aliases kept for one release.

#include "streamrcommon.h"

// Proxy-API-specific error codes (the shared ones are in streamrcommon.h).
#define ERROR_PROXY_CLIENT_NOT_FOUND "PROXY_CLIENT_NOT_FOUND"
#define ERROR_INVALID_PROXY_URL "INVALID_PROXY_URL"
#define ERROR_NO_PROXIES_DEFINED "NO_PROXIES_DEFINED"
#define ERROR_PROXY_CONNECTION_FAILED "PROXY_CONNECTION_FAILED"
#define ERROR_PROXY_BROADCAST_FAILED "PROXY_BROADCAST_FAILED"

EXTERN_C SHARED_EXPORT const char* testRpc(void);

// NOLINTNEXTLINE
SHARED_EXPORT static void __attribute__((constructor)) initialize(void);

// ---------------------------------------------------------------------------
// Deprecated pre-3.0 aliases of the shared infrastructure. New code (and
// the language wrappers) uses the streamrcommon.h names; these are kept
// for one release so existing proxy-API consumers keep compiling, and the
// old C symbols stay exported so existing binaries keep linking.
// ---------------------------------------------------------------------------

// NOLINTNEXTLINE
typedef StreamrPeer Proxy;
// NOLINTNEXTLINE
typedef StreamrError Error;
// NOLINTNEXTLINE
typedef StreamrResult ProxyResult;

/**
 * @deprecated Use streamrInitLibrary().
 */
// NOLINTNEXTLINE
EXTERN_C SHARED_EXPORT STREAMR_DEPRECATED(
    "use streamrInitLibrary()") void proxyClientInitLibrary(void);

/**
 * @deprecated Use streamrCleanupLibrary().
 */
// NOLINTNEXTLINE
EXTERN_C SHARED_EXPORT STREAMR_DEPRECATED(
    "use streamrCleanupLibrary()") void proxyClientCleanupLibrary(void);

/**
 * @deprecated Use streamrResultDelete().
 */
EXTERN_C SHARED_EXPORT
    STREAMR_DEPRECATED("use streamrResultDelete()") void proxyClientResultDelete(
        const StreamrResult* proxyResult);

// ---------------------------------------------------------------------------
// The proxy-client calls. These keep their names (the API itself is about
// proxies); only the shared types in their signatures changed spelling.
// ---------------------------------------------------------------------------

/**
 * @brief Create a new proxy client.
 *
 * @param streamrResult Pointer in which the StreamrResult will be stored.
 * You MUST call streamrResultDelete() on this after the call returns. The
 * resulting StreamrResult may only contain "errors" — the "successful"
 * and "numSuccessful" fields are unused.
 * @param ownEthereumAddress The Ethereum address of the client in format
 * 0x1234567890123456789012345678901234567890.
 * @param streamPartId The stream part id in format
 * 0xa000000000000000000000000000000000000000#01.
 * @return The handle of the created client.
 */

EXTERN_C SHARED_EXPORT uint64_t proxyClientNew(
    const StreamrResult** streamrResult,
    const char* ownEthereumAddress,
    const char* streamPartId);

/**
 * @brief Delete a proxy client.
 *
 * @param streamrResult Pointer in which the StreamrResult will be stored.
 * You MUST call streamrResultDelete() on this after the call returns. The
 * resulting StreamrResult may only contain "errors" — the "successful"
 * and "numSuccessful" fields are unused.
 * @param clientHandle The client handle of the client to delete.
 */

EXTERN_C SHARED_EXPORT void proxyClientDelete(
    const StreamrResult** streamrResult, uint64_t clientHandle);

/**
 * @brief Connect a proxy client to a list of proxies.
 *
 * @param streamrResult Pointer in which the StreamrResult will be stored.
 * You MUST call streamrResultDelete() on this after the call returns.
 * @param clientHandle The client handle of the client to connect.
 * @param proxies The array of proxies.
 * @param numProxies The number of proxies.
 * @return The number of proxies connected to.
 */

EXTERN_C SHARED_EXPORT uint64_t proxyClientConnect(
    const StreamrResult** streamrResult,
    uint64_t clientHandle,
    const StreamrPeer* proxies,
    uint64_t numProxies);

/**
 * @brief Publish a message to the stream.
 *
 * @param streamrResult Pointer in which the StreamrResult will be stored.
 * You MUST call streamrResultDelete() on this after the call returns.
 * @param clientHandle The client handle of the client to publish.
 * @param content The content to publish.
 * @param contentLength The length of the content.
 * @param ethereumPrivateKey as hex without 0x prefix (64 characters) or
 * NULL if the message should not be signed.
 * @return The number of proxies to which the message was published.
 */

EXTERN_C SHARED_EXPORT uint64_t proxyClientPublish(
    const StreamrResult** streamrResult,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength,
    const char* ethereumPrivateKey);

#endif /* LIBSTREAMRPROXYCLIENT_H */
