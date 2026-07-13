#ifndef STREAMR_NODE_H
#define STREAMR_NODE_H

// Public C API of the full Streamr network node (milestone D of
// trackerless-network-completion-plan.md).
//
// This API lives in the same shared library as the proxy-client API
// (streamrproxyclient.h) and reuses its infrastructure on purpose:
//   - Results are reported through the same const ProxyResult** out
//     parameter in every call; every returned result (success or error)
//     MUST be freed with proxyClientResultDelete().
//   - The library lifecycle functions proxyClientInitLibrary() /
//     proxyClientCleanupLibrary() govern this API too;
//     proxyClientCleanupLibrary() stops and deletes any nodes that are
//     still alive. As with the proxy API, it MUST be called before the
//     process exits (see the note in streamrproxyclient.h).
//   - Entry points are (websocketUrl, ethereumAddress) pairs with the
//     same layout as struct Proxy; the node id of an entry point is
//     derived from its ethereum address exactly like the proxy API
//     derives peer ids, so any streamrNode* node can act as an entry
//     point for another one.
// NAMING: whether the shared types/lifecycle should be renamed to a
// streamr* prefix (a breaking change for proxy-API users) is left as a
// review decision for this PR.

#include <stdbool.h> // NOLINT
#include "streamrproxyclient.h"

#define ERROR_NODE_NOT_FOUND "NODE_NOT_FOUND"
#define ERROR_NODE_NOT_STARTED "NODE_NOT_STARTED"
#define ERROR_NODE_ALREADY_STARTED "NODE_ALREADY_STARTED"
#define ERROR_NODE_STOPPED "NODE_STOPPED"
#define ERROR_INVALID_ENTRY_POINT_URL "INVALID_ENTRY_POINT_URL"
#define ERROR_SUBSCRIPTION_NOT_FOUND "SUBSCRIPTION_NOT_FOUND"
#define ERROR_NODE_OPERATION_FAILED "NODE_OPERATION_FAILED"

// An entry point has the same shape as a proxy: the websocket URL the
// node listens on plus the ethereum address its node id is derived
// from.
typedef Proxy StreamrEntryPoint;

typedef struct StreamrNodeConfig {
    // Entry points of the layer-0 DHT. May be NULL when numEntryPoints
    // is 0; the node then joins its own DHT and acts as the first entry
    // point of a new network (it should have a websocket server port in
    // that case so that others can join through it).
    const StreamrEntryPoint* entryPoints;
    uint64_t numEntryPoints;
    // Websocket server port. 0 = do not run a websocket server; the
    // node then connects out as a client (websocket client + WebRTC
    // connections), which is the mode intended for mobile devices.
    uint16_t websocketPort;
    // Host name or IP address the websocket server advertises in the
    // node's peer descriptor. NULL = "127.0.0.1". Ignored when
    // websocketPort is 0.
    const char* websocketHost;
    // Accept incoming proxy connections from proxy clients (a node with
    // this enabled cannot itself use streamrNodeSetProxies; the two
    // roles are mutually exclusive, as in the TypeScript
    // implementation).
    bool acceptProxyConnections;
} StreamrNodeConfig;

// Direction of a proxied stream part connection; the values match
// ProxyDirection in protos/NetworkRpc.proto.
typedef enum StreamrProxyDirection {
    STREAMR_PROXY_DIRECTION_PUBLISH = 0,
    STREAMR_PROXY_DIRECTION_SUBSCRIBE = 1
} StreamrProxyDirection;

// Message callback of streamrNodeSubscribe(). Invoked on an internal
// network thread: return quickly, and do NOT call streamrNode* /
// proxyClient* functions from inside the callback. content points to a
// buffer of contentLength bytes that is only valid during the call.
// streamPartId is in canonical form (partition without leading zeros,
// e.g. "0xabc...#1" even if the subscription said "#01").
typedef void (*StreamrNodeMessageCallback)(
    uint64_t nodeHandle,
    const char* streamPartId,
    const char* content,
    uint64_t contentLength,
    void* userData);

// Creates a network node (not yet started; no sockets are opened).
// ownEthereumAddress is the node's identity: its node id is derived
// from it, and it is used as the publisher id of published messages.
// config may be NULL, which is equivalent to a zeroed config (no entry
// points, no websocket server). Returns the node handle, or 0 on error.
EXTERN_C SHARED_EXPORT uint64_t streamrNodeNew(
    const ProxyResult** result,
    const char* ownEthereumAddress,
    const StreamrNodeConfig* config);

// Stops the node if it is still running and releases it. The handle is
// invalid afterwards.
EXTERN_C SHARED_EXPORT void streamrNodeDelete(
    const ProxyResult** result, uint64_t nodeHandle);

// Starts the node: opens the websocket server (if configured) and
// joins the layer-0 DHT through the configured entry points. Blocks
// until the node is connected to the network. A node cannot be started
// twice, and a stopped node cannot be restarted.
EXTERN_C SHARED_EXPORT void streamrNodeStart(
    const ProxyResult** result, uint64_t nodeHandle);

// Stops the node and leaves the network. Terminal: the node cannot be
// restarted (create a new one instead). Idempotent.
EXTERN_C SHARED_EXPORT void streamrNodeStop(
    const ProxyResult** result, uint64_t nodeHandle);

// Joins a stream part. streamPartEntryPoints optionally names nodes
// known to be in the stream part (same convention as the layer-0 entry
// points); when none are given the stream part's entry points are
// discovered through the DHT. Returns once the join has been initiated;
// use streamrNodeGetNeighborCount to observe the topology forming.
EXTERN_C SHARED_EXPORT void streamrNodeJoinStreamPart(
    const ProxyResult** result,
    uint64_t nodeHandle,
    const char* streamPartId,
    const StreamrEntryPoint* streamPartEntryPoints,
    uint64_t numStreamPartEntryPoints);

// Leaves a stream part.
EXTERN_C SHARED_EXPORT void streamrNodeLeaveStreamPart(
    const ProxyResult** result, uint64_t nodeHandle, const char* streamPartId);

// Publishes a message to a stream part (joining it first if needed).
// content is contentLength bytes of opaque payload. ethereumPrivateKey
// may be NULL for an unsigned message; when given, the message is
// signed with ECDSA (secp256k1, EVM style) exactly like
// proxyClientPublish. Fire-and-forget: delivery into the overlay is
// not acknowledged.
EXTERN_C SHARED_EXPORT void streamrNodePublish(
    const ProxyResult** result,
    uint64_t nodeHandle,
    const char* streamPartId,
    const char* content,
    uint64_t contentLength,
    const char* ethereumPrivateKey);

// Subscribes to a stream part (joining it first if needed): callback is
// invoked for every message received on it. Returns a subscription
// handle for streamrNodeUnsubscribe, or 0 on error.
EXTERN_C SHARED_EXPORT uint64_t streamrNodeSubscribe(
    const ProxyResult** result,
    uint64_t nodeHandle,
    const char* streamPartId,
    StreamrNodeMessageCallback callback,
    void* userData);

// Removes a subscription. The callback may still be invoked for
// messages whose dispatch already started when this returns.
EXTERN_C SHARED_EXPORT void streamrNodeUnsubscribe(
    const ProxyResult** result,
    uint64_t nodeHandle,
    uint64_t subscriptionHandle);

// Number of neighbors the node currently has in a stream part's
// delivery topology.
EXTERN_C SHARED_EXPORT uint64_t streamrNodeGetNeighborCount(
    const ProxyResult** result, uint64_t nodeHandle, const char* streamPartId);

// Connects the stream part through proxy nodes instead of joining its
// topology (the proxy mode of the old proxy-client API folded into the
// node handle). direction selects publishing or subscribing through
// the proxies; connectionCount 0 means "connect to all given proxies".
// Passing numProxies 0 disconnects the stream part from its proxies.
EXTERN_C SHARED_EXPORT void streamrNodeSetProxies(
    const ProxyResult** result,
    uint64_t nodeHandle,
    const char* streamPartId,
    const Proxy* proxies,
    uint64_t numProxies,
    StreamrProxyDirection direction,
    uint64_t connectionCount);

#endif
