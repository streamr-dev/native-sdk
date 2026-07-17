#ifndef STREAMR_COMMON_H
#define STREAMR_COMMON_H

// Shared infrastructure of the Streamr shared-library C APIs: the result
// and error types, the peer locator struct, and the library lifecycle.
// Used by both the full-node API (streamrnode.h) and the proxy-client API
// (streamrproxyclient.h). These are the FINAL, API-neutral names decided
// in phase D3 of trackerless-network-completion-plan.md; the former
// proxy-prefixed names remain available as deprecated aliases in
// streamrproxyclient.h for one release.

// NOLINTNEXTLINE
#include <stdint.h>

#if defined(__clang__) || defined(__GNUC__)
#define SHARED_EXPORT __attribute__((visibility("default")))
#define SHARED_LOCAL __attribute__((visibility("hidden")))
#define STREAMR_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
#define SHARED_EXPORT
#define SHARED_LOCAL
#define STREAMR_DEPRECATED(msg)
#endif

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif

// Error codes shared by both APIs.
#define ERROR_INVALID_ETHEREUM_ADDRESS "INVALID_ETHEREUM_ADDRESS"
#define ERROR_INVALID_STREAM_PART_ID "INVALID_STREAM_PART_ID"

/**
 * @brief A peer locator: the websocket URL a node listens on plus the
 * ethereum address its node id is derived from. Used both for the proxies
 * of the proxy-client API and for the entry points of the full-node API.
 */
// NOLINTNEXTLINE
typedef struct StreamrPeer {
    const char* websocketUrl;
    const char* ethereumAddress;
} StreamrPeer;

/**
 * @brief One error of a failed or partially failed call. code is one of
 * the ERROR_* constants; peer points at the peer the error concerns, or
 * NULL when the error is not peer-specific.
 */
// NOLINTNEXTLINE
typedef struct StreamrError {
    const char* message;
    const char* code;
    // The peer this error concerns. The member is accessible both as
    // `peer` (final name) and as `proxy` (the pre-3.0 spelling, kept for
    // source compatibility with proxy-API consumers; same storage).
    union {
        const struct StreamrPeer* peer;
        const struct StreamrPeer* proxy;
    };
} StreamrError;

/**
 * @brief The result of every call in both APIs, returned through a
 * const StreamrResult** out parameter. Every returned result — success or
 * error — MUST be freed with streamrResultDelete(). Calls that cannot
 * partially succeed leave `successful`/`numSuccessful` unused.
 */
// NOLINTNEXTLINE
typedef struct StreamrResult {
    StreamrError* errors;
    uint64_t numErrors;
    StreamrPeer* successful;
    uint64_t numSuccessful;
} StreamrResult;

/**
 * @brief Delete a StreamrResult. Must be called on every result returned
 * by any call of either API.
 */
EXTERN_C SHARED_EXPORT void streamrResultDelete(
    const StreamrResult* streamrResult);

/**
 * @brief Initialize the library. Called automatically when the library is
 * loaded, but can be called explicitly to force a re-initialization (e.g.
 * after streamrCleanupLibrary()).
 */
EXTERN_C SHARED_EXPORT void streamrInitLibrary(void);

/**
 * @brief Clean up the library. MUST be called before the program exits:
 * the standard dynamic-library destructor runs after static variables
 * have already been destroyed, too late to tear down the objects that
 * depend on them. Safe to call multiple times.
 */
EXTERN_C SHARED_EXPORT void streamrCleanupLibrary(void);

#endif /* STREAMR_COMMON_H */
