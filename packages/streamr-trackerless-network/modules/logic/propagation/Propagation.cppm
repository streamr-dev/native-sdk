// Module streamr.trackerlessnetwork.Propagation
// CONSOLIDATED from the former header logic/propagation/Propagation.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
//
// Async sends (phase C3): the TS sendToNeighbor is async and never
// blocks the caller; the earlier C++ port ran it synchronously, which
// meant a blockingWait inside the RPC delivery path — under broadcast
// fan-out the nodes blocked sending to each other in cycles and message
// propagation stalled (the 64-node Propagation test timeout). Sends are
// now Task-returning and run as bounded tasks on a GuardedAsyncScope;
// synchronous callers that need the outcome await collectResults().
module;

#include <coroutine> // IWYU pragma: keep

#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.Propagation;

import streamr.utils.CoroutineHelper;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.trackerlessnetwork.PropagationTaskStore;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::utils::GuardedAsyncScope;
export namespace streamr::trackerlessnetwork::propagation {

using ::dht::PeerDescriptor;
using SendToNeighborFn = std::function<folly::coro::Task<void>(
    const DhtAddress&, const StreamMessage&)>;

struct PropagationOptions {
    SendToNeighborFn sendToNeighbor;
    size_t minPropagationTargets;
    std::optional<std::chrono::milliseconds> ttl;
    std::optional<size_t> maxMessages;
};

inline constexpr size_t DEFAULT_MAX_MESSAGES = 150; // NOLINT
// NOLINTNEXTLINE
inline constexpr std::chrono::milliseconds DEFAULT_TTL =
    std::chrono::seconds(10);

/**
 * Message propagation logic of a node. Given a message, this class will
 * actively attempt to propagate it to `minPropagationTargets` neighbors until
 * success or TTL expiration.
 *
 * Setting `minPropagationTargets = 0` effectively disables any propagation
 * reattempts. A message will then only be propagated exactly once, to neighbors
 * that are present at that moment, in a fire-and-forget manner.
 */

class Propagation {
private:
    SendToNeighborFn sendToNeighbor;
    size_t minPropagationTargets;
    PropagationTaskStore activeTaskStore;
    // Serializes handledNeighbors bookkeeping (sends themselves run
    // concurrently and are bounded by the RPC timeouts inside
    // sendToNeighbor).
    std::mutex mutex;
    GuardedAsyncScope scope;

public:
    explicit Propagation(const PropagationOptions& options)
        : activeTaskStore(
              options.ttl.value_or(DEFAULT_TTL),
              options.maxMessages.value_or(DEFAULT_MAX_MESSAGES)) {
        this->sendToNeighbor = options.sendToNeighbor;
        this->minPropagationTargets = options.minPropagationTargets;
    }

    ~Propagation() { this->stop(); }

    void stop() { this->scope.close(); }

    /**
     * Node should invoke this when it learns about a new message.
     * Targets are node ids; sends run detached (TS parity — the caller,
     * often an RPC delivery handler, must not block on them).
     */
    void feedUnseenMessage(
        const StreamMessage& message,
        const std::vector<DhtAddress>& targets,
        const std::optional<DhtAddress>& source) {
        auto task = std::make_shared<PropagationTask>(PropagationTask{
            .message = message,
            .source = source,
            .handledNeighbors = std::set<DhtAddress>()});
        this->activeTaskStore.add(*task);
        for (const auto& neighborId : targets) {
            this->scheduleSend(task, neighborId);
        }
    }

    /**
     * Awaitable variant for callers that need the per-target outcome
     * (the proxy-client shared-library API): resolves once every send
     * has completed, with the (failed, successful) id lists.
     */
    folly::coro::Task<
        std::pair<std::vector<DhtAddress>, std::vector<DhtAddress>>>
    feedUnseenMessageAndCollect(
        StreamMessage message,
        std::vector<DhtAddress> targets,
        std::optional<DhtAddress> source) {
        auto task = std::make_shared<PropagationTask>(PropagationTask{
            .message = std::move(message),
            .source = std::move(source),
            .handledNeighbors = std::set<DhtAddress>()});
        this->activeTaskStore.add(*task);
        std::vector<folly::coro::Task<bool>> sends;
        sends.reserve(targets.size());
        for (const auto& neighborId : targets) {
            sends.push_back(this->sendAndAwaitThenMark(task, neighborId));
        }
        // (task is a shared_ptr; every send marks the same set.)
        const auto results =
            co_await folly::coro::collectAllTryRange(std::move(sends));
        std::vector<DhtAddress> failed;
        std::vector<DhtAddress> successful;
        for (size_t i = 0; i < results.size(); i++) {
            if (results[i].hasValue() && results[i].value()) {
                successful.push_back(targets[i]);
            } else {
                failed.push_back(targets[i]);
            }
        }
        co_return {failed, successful};
    }

    /**
     * Node should invoke this when it learns about a new node stream
     * assignment.
     */
    void onNeighborJoined(const DhtAddress& neighborId) {
        // store.get() returns snapshots; each retry marks its own copy
        // (mirrors the pre-existing TS-comment semantics).
        for (const auto& task : this->activeTaskStore.get()) {
            this->scheduleSend(
                std::make_shared<PropagationTask>(task), neighborId);
        }
    }

private:
    void scheduleSend(
        std::shared_ptr<PropagationTask> task, DhtAddress neighborId) {
        this->scope.add(
            streamr::utils::co_withExecutor(
                &streamr::utils::SharedExecutors::worker(),
                folly::coro::co_invoke(
                    [this,
                     task = std::move(task),
                     neighborId =
                         std::move(neighborId)]() -> folly::coro::Task<void> {
                        co_await this->sendAndAwaitThenMark(task, neighborId);
                    })));
    }

    folly::coro::Task<bool> sendAndAwaitThenMark(
        std::shared_ptr<PropagationTask> task, DhtAddress neighborId) {
        {
            std::scoped_lock lock(this->mutex);
            if (task->handledNeighbors.contains(neighborId) ||
                neighborId == task->source) {
                co_return false;
            }
        }
        try {
            co_await this->sendToNeighbor(neighborId, task->message);
        } catch (...) {
            co_return false;
        }
        // Side-note: due to asynchronicity, the task being modified at
        // this point could already be stale and deleted from
        // `activeTaskStore`. However, as modifying it or re-deleting it
        // is pretty much inconsequential at this point, leaving the
        // logic as is (mirrors the TS comment).
        {
            std::scoped_lock lock(this->mutex);
            task->handledNeighbors.insert(neighborId);
            if (task->handledNeighbors.size() >= this->minPropagationTargets) {
                this->activeTaskStore.remove(
                    PropagationTaskStore::messageIdToMessageRef(
                        task->message.messageid()));
            }
        }
        co_return true;
    }
};

} // namespace streamr::trackerlessnetwork::propagation
