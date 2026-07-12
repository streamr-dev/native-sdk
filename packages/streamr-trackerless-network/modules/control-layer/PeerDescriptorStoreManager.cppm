// Module streamr.trackerlessnetwork.PeerDescriptorStoreManager
// Ported from packages/trackerless-network/src/control-layer/
// PeerDescriptorStoreManager.ts (v103.8.0-rc.3): for each key there are
// usually 0..MAX_NODE_COUNT PeerDescriptors stored in the DHT; if there
// are fewer nodes, the local node's peer descriptor is stored (and then
// periodically re-stored) under the key. The DHT operations are taken as
// callbacks (TS options style) so the class stays independent of DhtNode
// and the unit test can substitute fakes.
//
// The public entry points are virtual (unlike TS, which force-casts an
// unrelated fake): StreamPartReconnect's test substitutes
// FakePeerDescriptorStoreManager by overriding them.
module;

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <coroutine> // IWYU pragma: keep

#include <google/protobuf/any.pb.h>

export module streamr.trackerlessnetwork.PeerDescriptorStoreManager;

import streamr.dht.protos;

import streamr.dht.Identifiers;
import streamr.logger.SLogger;
import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.SharedExecutors;

// Hoisted from the former-header idiom (file scope, NOT exported).
using streamr::logger::SLogger;

namespace {

// TS parsePeerDescriptor: drop tombstoned entries, unpack the rest.
std::vector<::dht::PeerDescriptor> parsePeerDescriptors(
    const std::vector<::dht::DataEntry>& dataEntries) {
    std::vector<::dht::PeerDescriptor> result;
    for (const auto& entry : dataEntries) {
        if (entry.deleted()) {
            continue;
        }
        ::dht::PeerDescriptor peerDescriptor;
        if (entry.data().UnpackTo(&peerDescriptor)) {
            result.push_back(std::move(peerDescriptor));
        }
    }
    return result;
}

} // namespace

export namespace streamr::trackerlessnetwork::controllayer {

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using ::google::protobuf::Any;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::utils::AbortController;

constexpr size_t maxNodeCount = 8; // TS MAX_NODE_COUNT

struct PeerDescriptorStoreManagerOptions {
    DhtAddress key;
    PeerDescriptor localPeerDescriptor;
    std::optional<std::chrono::milliseconds> storeInterval;
    std::function<folly::coro::Task<std::vector<DataEntry>>(DhtAddress)>
        fetchDataFromDht;
    std::function<folly::coro::Task<std::vector<PeerDescriptor>>(
        DhtAddress, Any)>
        storeDataToDht;
    std::function<folly::coro::Task<void>(DhtAddress, bool)> deleteDataFromDht;
};

class PeerDescriptorStoreManager {
private:
    static constexpr std::chrono::milliseconds defaultStoreInterval{60000};

    PeerDescriptorStoreManagerOptions options;
    AbortController abortController;
    bool localNodeStored = false;
    // Owns the keep-alive interval loop. TS detaches it (scheduleAtInterval)
    // and lets GC collect the closure; in C++ a detached loop touching
    // `this` is a use-after-free once the manager dies, so the loop lives
    // in a scope drained by destroy()/the destructor (see the
    // teardown-drain-ordering lessons: the drain is awaited, never a
    // blocking join on a pool thread).
    folly::coro::CancellableAsyncScope keepAliveScope;
    std::atomic<bool> keepAliveScopeDrained = false;

    folly::coro::Task<void> storeLocalNode() {
        Any dataToStore;
        dataToStore.PackFrom(this->options.localPeerDescriptor);
        try {
            co_await this->options.storeDataToDht(
                this->options.key, std::move(dataToStore));
        } catch (const std::exception& err) {
            SLogger::warn(
                "Failed to store local node, key: " + this->options.key + " (" +
                err.what() + ")");
        }
    }

    folly::coro::Task<void> keepLocalNodeAttempt() {
        SLogger::trace(
            "Attempting to keep local node, key: " + this->options.key);
        try {
            const auto discovered = co_await this->fetchNodes();
            const auto isLocal = [this](const PeerDescriptor& peerDescriptor) {
                return Identifiers::areEqualPeerDescriptors(
                    peerDescriptor, this->options.localPeerDescriptor);
            };
            if (discovered.size() < maxNodeCount ||
                std::ranges::any_of(discovered, isLocal)) {
                co_await this->storeLocalNode();
            }
        } catch (const std::exception& err) {
            SLogger::debug(
                "Failed to keep local node, key: " + this->options.key + " (" +
                err.what() + ")");
        }
    }

    // TS keepLocalNode = scheduleAtInterval(task, interval, false, signal):
    // resolves immediately and the interval loop runs detached. Here the
    // loop is a keepAliveScope task so teardown can drain it (see the
    // member comment).
    void keepLocalNode() {
        const auto token =
            this->abortController.getSignal().getCancellationToken();
        this->keepAliveScope.add(
            streamr::utils::co_withExecutor(
                &streamr::utils::SharedExecutors::worker(),
                folly::coro::co_invoke(
                    [this, token]() -> folly::coro::Task<void> {
                        const auto interval =
                            this->options.storeInterval.value_or(
                                defaultStoreInterval);
                        while (!token.isCancellationRequested()) {
                            try {
                                co_await streamr::utils::co_withCancellation(
                                    token, folly::coro::sleep(interval));
                            } catch (const std::exception&) {
                                co_return; // aborted mid-sleep
                            }
                            if (token.isCancellationRequested()) {
                                co_return;
                            }
                            co_await this->keepLocalNodeAttempt();
                        }
                    })));
    }

public:
    explicit PeerDescriptorStoreManager(
        PeerDescriptorStoreManagerOptions options)
        : options(std::move(options)) {}

    virtual ~PeerDescriptorStoreManager() { this->drainKeepAliveScope(); }
    PeerDescriptorStoreManager(const PeerDescriptorStoreManager&) = delete;
    PeerDescriptorStoreManager& operator=(const PeerDescriptorStoreManager&) =
        delete;
    PeerDescriptorStoreManager(PeerDescriptorStoreManager&&) = delete;
    PeerDescriptorStoreManager& operator=(PeerDescriptorStoreManager&&) =
        delete;

    virtual folly::coro::Task<std::vector<PeerDescriptor>> fetchNodes() {
        SLogger::trace("Fetch data, key: " + this->options.key);
        try {
            const auto result =
                co_await this->options.fetchDataFromDht(this->options.key);
            co_return parsePeerDescriptors(result);
        } catch (const std::exception&) {
            co_return std::vector<PeerDescriptor>{};
        }
    }

    virtual folly::coro::Task<void> storeAndKeepLocalNode() {
        if (this->abortController.getSignal().aborted) {
            co_return;
        }
        this->localNodeStored = true;
        co_await this->storeLocalNode();
        this->keepLocalNode();
    }

    [[nodiscard]] virtual bool isLocalNodeStored() const {
        return this->localNodeStored;
    }

    virtual folly::coro::Task<void> destroy() {
        this->abortController.abort();
        co_await this->options.deleteDataFromDht(this->options.key, false);
        // Awaited (never a blocking join): safe on any thread, including
        // pool workers.
        if (!this->keepAliveScopeDrained.exchange(true)) {
            co_await this->keepAliveScope.cancelAndJoinAsync();
        }
    }

protected:
    // Backstop for owners that never call destroy(). Blocking: must run on
    // an owner thread, never a shared-pool worker (the repo-wide
    // communicator-teardown contract). Aborts first — the loop's sleep is
    // cancelled by the abort token (which shadows the scope token), so a
    // join without the abort would wait out the full store interval.
    void drainKeepAliveScope() noexcept {
        if (!this->keepAliveScopeDrained.exchange(true)) {
            this->abortController.abort();
            try {
                streamr::utils::blockingWait(
                    this->keepAliveScope.cancelAndJoinAsync());
            } catch (...) { // NOLINT(bugprone-empty-catch) must not throw
            }
        }
    }
};

} // namespace streamr::trackerlessnetwork::controllayer
