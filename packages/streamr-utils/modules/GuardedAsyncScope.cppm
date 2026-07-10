// Module streamr.utils.GuardedAsyncScope
//
// A folly::coro::AsyncScope with a close() gate. folly forbids add()
// racing joinAsync(), but several owners (DhtNode recovery, PeerDiscovery
// rejoin, StoreManager replication, Router sessions) spawn detached tasks
// from event handlers that can still fire while stop() is draining the
// scope. add() and the close() flag flip serialize on an internal mutex,
// so an add either lands fully before the join starts or is dropped —
// dropping is correct there because the owner has already aborted the
// work the task would do (the former per-instance executors had the same
// effective cutoff at their destructor join).
module;

#include <mutex>
#include <utility>

#include <coroutine> // IWYU pragma: keep

export module streamr.utils.GuardedAsyncScope;

import streamr.utils.CoroutineHelper;

export namespace streamr::utils {

class GuardedAsyncScope {
private:
    folly::coro::AsyncScope scope;
    std::mutex mutex;
    bool closed = false;

public:
    // Starts `awaitable` detached, tracked by this scope; dropped silently
    // if the scope is already closed. Pass a TaskWithExecutor
    // (co_withExecutor(...)) so the task starts on its executor, not the
    // calling thread.
    template <typename Awaitable>
    void add(Awaitable&& awaitable) {
        std::scoped_lock lock(this->mutex);
        if (this->closed) {
            return;
        }
        this->scope.add(std::forward<Awaitable>(awaitable));
    }

    // Blocks until every added task has finished; further adds are
    // dropped. Idempotent for a single owning stopper (the repo's stop()
    // methods are already guarded against double invocation). The caller
    // aborts/cancels its tasks BEFORE closing so the drain returns
    // promptly, and must not call this from a thread the tasks need to
    // finish on.
    void close() {
        {
            std::scoped_lock lock(this->mutex);
            if (this->closed) {
                return;
            }
            this->closed = true;
        }
        streamr::utils::blockingWait(this->scope.joinAsync());
    }

    ~GuardedAsyncScope() { this->close(); }

    GuardedAsyncScope() = default;
    GuardedAsyncScope(const GuardedAsyncScope&) = delete;
    GuardedAsyncScope& operator=(const GuardedAsyncScope&) = delete;
    GuardedAsyncScope(GuardedAsyncScope&&) = delete;
    GuardedAsyncScope& operator=(GuardedAsyncScope&&) = delete;
};

} // namespace streamr::utils
