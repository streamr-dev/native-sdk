#ifndef STREAMR_NODE_HPP
#define STREAMR_NODE_HPP

// C++ wrapper of the full-node C API (streamrnode.h) — phase D3b of
// trackerless-network-completion-plan.md. Reuses the shared result/error
// plumbing of StreamrProxyClient.hpp.

#include <streamrnode.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "StreamrProxyClient.hpp"

namespace streamrproxyclient {

struct ErrorNodeNotFound {
    static constexpr const char* name = ERROR_NODE_NOT_FOUND;
};
struct ErrorNodeNotStarted {
    static constexpr const char* name = ERROR_NODE_NOT_STARTED;
};
struct ErrorNodeAlreadyStarted {
    static constexpr const char* name = ERROR_NODE_ALREADY_STARTED;
};
struct ErrorNodeStopped {
    static constexpr const char* name = ERROR_NODE_STOPPED;
};
struct ErrorInvalidEntryPointUrl {
    static constexpr const char* name = ERROR_INVALID_ENTRY_POINT_URL;
};
struct ErrorSubscriptionNotFound {
    static constexpr const char* name = ERROR_SUBSCRIPTION_NOT_FOUND;
};
struct ErrorNodeOperationFailed {
    static constexpr const char* name = ERROR_NODE_OPERATION_FAILED;
};

using StreamrNodeErrorCode = std::variant<
    ErrorInvalidEthereumAddress,
    ErrorInvalidStreamPartId,
    ErrorNodeNotFound,
    ErrorNodeNotStarted,
    ErrorNodeAlreadyStarted,
    ErrorNodeStopped,
    ErrorInvalidEntryPointUrl,
    ErrorSubscriptionNotFound,
    ErrorNodeOperationFailed>;

/**
 * @brief Exception thrown by StreamrNode operations.
 */
struct StreamrNodeError : public std::runtime_error {
    StreamrNodeErrorCode code;
    /** The peer the error concerns; empty when not peer-specific. */
    StreamrProxyAddress peer;

    explicit StreamrNodeError(const StreamrError* error)
        : std::runtime_error(error->message), code(getErrorCode(error->code)) {
        if (error->peer) {
            peer = StreamrProxyAddress::fromCProxy(error->peer);
        }
    }

private:
    static StreamrNodeErrorCode getErrorCode(const char* code) {
        if (strcmp(code, ERROR_INVALID_ETHEREUM_ADDRESS) == 0) {
            return ErrorInvalidEthereumAddress{};
        }
        if (strcmp(code, ERROR_INVALID_STREAM_PART_ID) == 0) {
            return ErrorInvalidStreamPartId{};
        }
        if (strcmp(code, ERROR_NODE_NOT_FOUND) == 0) {
            return ErrorNodeNotFound{};
        }
        if (strcmp(code, ERROR_NODE_NOT_STARTED) == 0) {
            return ErrorNodeNotStarted{};
        }
        if (strcmp(code, ERROR_NODE_ALREADY_STARTED) == 0) {
            return ErrorNodeAlreadyStarted{};
        }
        if (strcmp(code, ERROR_NODE_STOPPED) == 0) {
            return ErrorNodeStopped{};
        }
        if (strcmp(code, ERROR_INVALID_ENTRY_POINT_URL) == 0) {
            return ErrorInvalidEntryPointUrl{};
        }
        if (strcmp(code, ERROR_SUBSCRIPTION_NOT_FOUND) == 0) {
            return ErrorSubscriptionNotFound{};
        }
        return ErrorNodeOperationFailed{};
    }
};

/**
 * @brief Options of a StreamrNode; mirrors StreamrNodeConfig with owned
 * storage. Zero-initialized defaults mean: no entry points (the node
 * starts a new network and acts as its first entry point), no websocket
 * server (client-only operation — the mobile mode).
 */
struct StreamrNodeOptions {
    std::vector<StreamrProxyAddress> entryPoints;
    uint16_t websocketPort = 0;
    std::string websocketHost; /**< empty = default (127.0.0.1) */
    bool acceptProxyConnections = false;
};

/**
 * @brief RAII wrapper of the full-node C API: create/start/stop a node,
 * join stream parts, publish, subscribe with a std::function callback.
 * Errors are thrown as StreamrNodeError.
 */
class StreamrNode {
public:
    /**
     * @brief Message callback of subscribe(). Invoked on an internal
     * network thread: return quickly and do NOT call StreamrNode methods
     * from inside the callback. Both views are only valid during the
     * call.
     */
    using MessageCallback =
        void(std::string_view streamPartId, std::string_view content);

    explicit StreamrNode(
        const std::string& ownEthereumAddress,
        const StreamrNodeOptions& options = {}) {
        std::vector<StreamrEntryPoint> entryPoints;
        entryPoints.reserve(options.entryPoints.size());
        for (const auto& entryPoint : options.entryPoints) {
            entryPoints.push_back(
                {.websocketUrl = entryPoint.websocketUrl.c_str(),
                 .ethereumAddress = entryPoint.ethereumAddress.c_str()});
        }
        const StreamrNodeConfig config{
            .entryPoints = entryPoints.empty() ? nullptr : entryPoints.data(),
            .numEntryPoints = entryPoints.size(),
            .websocketPort = options.websocketPort,
            .websocketHost = options.websocketHost.empty()
                ? nullptr
                : options.websocketHost.c_str(),
            .acceptProxyConnections = options.acceptProxyConnections};
        const StreamrResult* result = nullptr;
        this->nodeHandle =
            streamrNodeNew(&result, ownEthereumAddress.c_str(), &config);
        checkResult(result);
    }

    /**
     * @brief Stops the node (if running) and releases it.
     */
    ~StreamrNode() {
        if (this->nodeHandle != 0) {
            const StreamrResult* result = nullptr;
            streamrNodeDelete(&result, this->nodeHandle);
            streamrResultDelete(result);
        }
    }

    StreamrNode(const StreamrNode&) = delete;
    StreamrNode& operator=(const StreamrNode&) = delete;
    StreamrNode(StreamrNode&&) = delete;
    StreamrNode& operator=(StreamrNode&&) = delete;

    /**
     * @brief Starts the node and joins the layer-0 network; blocks until
     * connected. A node cannot be started twice or restarted after stop.
     */
    void start() const {
        const StreamrResult* result = nullptr;
        streamrNodeStart(&result, this->nodeHandle);
        checkResult(result);
    }

    /**
     * @brief Stops the node and leaves the network. Terminal; idempotent.
     */
    void stop() const {
        const StreamrResult* result = nullptr;
        streamrNodeStop(&result, this->nodeHandle);
        checkResult(result);
    }

    void joinStreamPart(
        const std::string& streamPartId,
        const std::vector<StreamrProxyAddress>& streamPartEntryPoints = {})
        const {
        std::vector<StreamrEntryPoint> entryPoints;
        entryPoints.reserve(streamPartEntryPoints.size());
        for (const auto& entryPoint : streamPartEntryPoints) {
            entryPoints.push_back(
                {.websocketUrl = entryPoint.websocketUrl.c_str(),
                 .ethereumAddress = entryPoint.ethereumAddress.c_str()});
        }
        const StreamrResult* result = nullptr;
        streamrNodeJoinStreamPart(
            &result,
            this->nodeHandle,
            streamPartId.c_str(),
            entryPoints.empty() ? nullptr : entryPoints.data(),
            entryPoints.size());
        checkResult(result);
    }

    void leaveStreamPart(const std::string& streamPartId) const {
        const StreamrResult* result = nullptr;
        streamrNodeLeaveStreamPart(
            &result, this->nodeHandle, streamPartId.c_str());
        checkResult(result);
    }

    /**
     * @brief Publishes a message (joining the stream part first if
     * needed). Pass an empty ethereumPrivateKey for an unsigned message;
     * otherwise hex without 0x prefix, as in proxyClientPublish.
     */
    void publish(
        const std::string& streamPartId,
        const std::string& content,
        const std::string& ethereumPrivateKey = {}) const {
        const StreamrResult* result = nullptr;
        streamrNodePublish(
            &result,
            this->nodeHandle,
            streamPartId.c_str(),
            content.data(),
            content.size(),
            ethereumPrivateKey.empty() ? nullptr : ethereumPrivateKey.c_str());
        checkResult(result);
    }

    /**
     * @brief Subscribes to a stream part (joining it first if needed).
     * @return A subscription handle for unsubscribe().
     * @details The callback stays owned by this node until the node is
     * destroyed (not merely until unsubscribe(): the C API may still
     * dispatch a message whose delivery already started when
     * unsubscribe returns).
     */
    uint64_t subscribe(
        const std::string& streamPartId,
        std::function<MessageCallback> callback) {
        auto ownedCallback = std::make_unique<std::function<MessageCallback>>(
            std::move(callback));
        const StreamrResult* result = nullptr;
        const uint64_t subscriptionHandle = streamrNodeSubscribe(
            &result,
            this->nodeHandle,
            streamPartId.c_str(),
            &StreamrNode::messageTrampoline,
            ownedCallback.get());
        checkResult(result);
        this->subscriptionCallbacks.push_back(std::move(ownedCallback));
        return subscriptionHandle;
    }

    void unsubscribe(uint64_t subscriptionHandle) const {
        const StreamrResult* result = nullptr;
        streamrNodeUnsubscribe(&result, this->nodeHandle, subscriptionHandle);
        checkResult(result);
    }

    [[nodiscard]] uint64_t neighborCount(
        const std::string& streamPartId) const {
        const StreamrResult* result = nullptr;
        const uint64_t count = streamrNodeGetNeighborCount(
            &result, this->nodeHandle, streamPartId.c_str());
        checkResult(result);
        return count;
    }

    /**
     * @brief Connects the stream part through proxies instead of joining
     * its topology; an empty proxy list disconnects it. connectionCount 0
     * means "connect to all given proxies".
     */
    void setProxies(
        const std::string& streamPartId,
        const std::vector<StreamrProxyAddress>& proxies,
        StreamrProxyDirection direction,
        uint64_t connectionCount = 0) const {
        std::vector<StreamrPeer> cProxies;
        cProxies.reserve(proxies.size());
        for (const auto& proxy : proxies) {
            cProxies.push_back(
                {.websocketUrl = proxy.websocketUrl.c_str(),
                 .ethereumAddress = proxy.ethereumAddress.c_str()});
        }
        const StreamrResult* result = nullptr;
        streamrNodeSetProxies(
            &result,
            this->nodeHandle,
            streamPartId.c_str(),
            cProxies.empty() ? nullptr : cProxies.data(),
            cProxies.size(),
            direction,
            connectionCount);
        checkResult(result);
    }

private:
    static SharedLibraryWrapper sharedLibraryWrapper;
    uint64_t nodeHandle = 0;
    std::vector<std::unique_ptr<std::function<MessageCallback>>>
        subscriptionCallbacks;

    static void messageTrampoline(
        uint64_t /*nodeHandle*/,
        const char* streamPartId,
        const char* content,
        uint64_t contentLength,
        void* userData) {
        const auto* callback =
            static_cast<std::function<MessageCallback>*>(userData);
        (*callback)(
            std::string_view(streamPartId),
            std::string_view(content, contentLength));
    }

    // Throws the first error (deleting the result first); frees the
    // result in every case.
    static void checkResult(const StreamrResult* result) {
        if (result != nullptr && result->numErrors > 0) {
            const auto error = StreamrNodeError(&result->errors[0]);
            streamrResultDelete(result);
            // The result must be freed BEFORE throwing, so the error is
            // built first and thrown as a copy — the same pattern the
            // proxy-client wrapper uses.
            // NOLINTNEXTLINE(misc-throw-by-value-catch-by-reference)
            throw error;
        }
        streamrResultDelete(result);
    }
};

// The single definition of the static member for header-only use.
inline SharedLibraryWrapper StreamrNode::sharedLibraryWrapper; // NOLINT

} // namespace streamrproxyclient
#endif // STREAMR_NODE_HPP
