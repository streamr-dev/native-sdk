// Module streamr.utils.scheduleAtInterval
// Ported from packages/utils/src/scheduleAtInterval.ts (v103.8.0-rc.3).
// Runs `task` once immediately (when executeAtStart) and then repeatedly,
// waiting `interval` after each completion, until `abortSignal` fires.
//
// TS schedules the recurring runs with setTimeout and the returned promise
// resolves after the first (executeAtStart) run. This port mirrors that: the
// returned Task completes after the executeAtStart run, and the recurring
// `sleep(interval)` then `task()` loop is detached onto `executor` — a worker
// pool, so this maintenance work never runs on a caller/delivery thread —
// stopping as soon as the abort signal requests cancellation.
module;

#include <chrono>
#include <functional>
#include <utility>

#include <coroutine> // IWYU pragma: keep

export module streamr.utils.scheduleAtInterval;

import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.ExecutorHelper;

export namespace streamr::utils {

using streamr::utils::AbortSignal;

inline folly::coro::Task<void> scheduleAtInterval(
    std::function<folly::coro::Task<void>()> task,
    std::chrono::milliseconds interval,
    bool executeAtStart,
    AbortSignal& abortSignal,
    folly::Executor* executor) {
    if (abortSignal.aborted) {
        co_return;
    }
    if (executeAtStart) {
        co_await task();
    }
    const auto cancellationToken = abortSignal.getCancellationToken();
    streamr::utils::co_withExecutor(
        executor,
        streamr::utils::co_withCancellation(
            cancellationToken,
            folly::coro::co_invoke(
                [task = std::move(task), interval, cancellationToken]()
                    -> folly::coro::Task<void> {
                    while (!cancellationToken.isCancellationRequested()) {
                        co_await folly::coro::sleep(interval);
                        if (cancellationToken.isCancellationRequested()) {
                            co_return;
                        }
                        co_await task();
                    }
                })))
        .start();
    co_return;
}

} // namespace streamr::utils
