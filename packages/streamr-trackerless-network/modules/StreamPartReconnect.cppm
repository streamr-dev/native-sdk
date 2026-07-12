// Module streamr.trackerlessnetwork.StreamPartReconnect
// Ported from packages/trackerless-network/src/StreamPartReconnect.ts
// (v103.8.0-rc.3): after losing all neighbors, periodically re-fetches the
// stream's entry points from the DHT and rejoins the layer-1 network until
// a neighbor is found (the interval task then aborts itself).
//
// Port note: TS runs the loop via scheduleAtInterval and lets GC collect
// the detached closure; here the loop is owned by a scope that reconnect()
// re-arms and destroy() drains (awaited, never a blocking join on a pool
// thread) so no straggler iteration can outlive this object or the
// discovery layer node it references.
module;

#include <atomic>
#include <chrono>
#include <exception>
#include <memory>

#include <coroutine> // IWYU pragma: keep

export module streamr.trackerlessnetwork.StreamPartReconnect;

import streamr.dht.protos;

import streamr.logger.SLogger;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.trackerlessnetwork.PeerDescriptorStoreManager;
import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.SharedExecutors;

// Hoisted from the former-header idiom (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::trackerlessnetwork {

using streamr::trackerlessnetwork::controllayer::maxNodeCount;
using streamr::trackerlessnetwork::controllayer::PeerDescriptorStoreManager;
using streamr::trackerlessnetwork::discoverylayer::DiscoveryLayerNode;
using streamr::utils::AbortController;

class StreamPartReconnect {
private:
    static constexpr std::chrono::milliseconds defaultReconnectInterval{
        30 * 1000};

    DiscoveryLayerNode& discoveryLayerNode;
    PeerDescriptorStoreManager& peerDescriptorStoreManager;
    // Recreated by each reconnect() like TS; null until the first call.
    std::unique_ptr<AbortController> abortController;
    folly::coro::CancellableAsyncScope scope;
    std::atomic<bool> scopeDrained = false;

    folly::coro::Task<void> reconnectAttempt() {
        const auto entryPoints =
            co_await this->peerDescriptorStoreManager.fetchNodes();
        co_await this->discoveryLayerNode.joinDht(entryPoints);
        // Is it necessary to store the node as an entry point here? (TS
        // carries the same open question.)
        if (!this->peerDescriptorStoreManager.isLocalNodeStored() &&
            entryPoints.size() < maxNodeCount) {
            co_await this->peerDescriptorStoreManager.storeAndKeepLocalNode();
        }
        if (this->discoveryLayerNode.getNeighborCount() > 0) {
            this->abortController->abort();
        }
    }

public:
    StreamPartReconnect(
        DiscoveryLayerNode& discoveryLayerNode,
        PeerDescriptorStoreManager& peerDescriptorStoreManager)
        : discoveryLayerNode(discoveryLayerNode),
          peerDescriptorStoreManager(peerDescriptorStoreManager) {}

    ~StreamPartReconnect() { this->drainScope(); }
    StreamPartReconnect(const StreamPartReconnect&) = delete;
    StreamPartReconnect& operator=(const StreamPartReconnect&) = delete;
    StreamPartReconnect(StreamPartReconnect&&) = delete;
    StreamPartReconnect& operator=(StreamPartReconnect&&) = delete;

    // TS scheduleAtInterval(task, timeout, true, signal): the first attempt
    // is awaited by the caller, the recurring attempts run detached until a
    // neighbor is found (the attempt aborts the controller) or destroy().
    folly::coro::Task<void> reconnect(
        std::chrono::milliseconds timeout = defaultReconnectInterval) {
        if (this->scopeDrained) {
            co_return; // destroyed; the joined scope cannot take new tasks
        }
        this->abortController = std::make_unique<AbortController>();
        const auto token =
            this->abortController->getSignal().getCancellationToken();
        co_await this->reconnectAttempt();
        this->scope.add(
            streamr::utils::co_withExecutor(
                &streamr::utils::SharedExecutors::worker(),
                folly::coro::co_invoke(
                    [this, token, timeout]() -> folly::coro::Task<void> {
                        while (!token.isCancellationRequested()) {
                            try {
                                co_await streamr::utils::co_withCancellation(
                                    token, folly::coro::sleep(timeout));
                            } catch (const std::exception&) {
                                co_return; // aborted mid-sleep
                            }
                            if (token.isCancellationRequested()) {
                                co_return;
                            }
                            try {
                                co_await this->reconnectAttempt();
                            } catch (const std::exception& err) {
                                SLogger::debug(
                                    "reconnect attempt failed: " +
                                    std::string(err.what()));
                            }
                        }
                    })));
    }

    [[nodiscard]] bool isRunning() const {
        return this->abortController != nullptr &&
            !this->abortController->getSignal().aborted;
    }

    void destroy() {
        if (this->abortController != nullptr) {
            this->abortController->abort();
        }
        this->drainScope();
    }

private:
    // Blocking drain: must run on an owner thread, never a shared-pool
    // worker (the repo-wide communicator-teardown contract). The loop's
    // sleep is cancelled by the abort token, so abort() first keeps this
    // prompt.
    void drainScope() noexcept {
        if (!this->scopeDrained.exchange(true)) {
            if (this->abortController != nullptr) {
                this->abortController->abort();
            }
            try {
                streamr::utils::blockingWait(this->scope.cancelAndJoinAsync());
            } catch (...) { // NOLINT(bugprone-empty-catch) must not throw
            }
        }
    }
};

} // namespace streamr::trackerlessnetwork
