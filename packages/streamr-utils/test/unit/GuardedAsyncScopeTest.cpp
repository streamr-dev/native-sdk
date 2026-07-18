#include <atomic>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;

using streamr::utils::blockingWait;
using streamr::utils::GuardedAsyncScope;
using streamr::utils::SharedExecutors;

namespace {

auto onWorker(folly::coro::Task<void>&& task) {
    return streamr::utils::co_withExecutor(
        &SharedExecutors::worker(), std::move(task));
}

} // namespace

TEST(GuardedAsyncScopeTest, CloseAsyncDrainsAddedTasks) {
    GuardedAsyncScope scope;
    std::atomic<bool> ran{false};
    scope.add(
        onWorker(folly::coro::co_invoke([&ran]() -> folly::coro::Task<void> {
            ran = true;
            co_return;
        })));
    blockingWait(scope.closeAsync());
    EXPECT_EQ(ran.load(), true);
}

TEST(GuardedAsyncScopeTest, AddAfterCloseAsyncIsDropped) {
    GuardedAsyncScope scope;
    blockingWait(scope.closeAsync());
    std::atomic<bool> ran{false};
    scope.add(
        onWorker(folly::coro::co_invoke([&ran]() -> folly::coro::Task<void> {
            ran = true;
            co_return;
        })));
    // close() after closeAsync() must stay a no-op (the destructor path).
    scope.close();
    EXPECT_EQ(ran.load(), false);
}

// The full-node teardown regression: several stop() coroutines share one
// blockingWait drive thread (collectAllRange), and the first scope's
// drain only finishes after a SIBLING stop has made progress. A
// thread-blocking close() deadlocks this shape; closeAsync() suspends,
// letting the sibling run, so the whole teardown completes.
TEST(GuardedAsyncScopeTest, CloseAsyncSuspendsSoSiblingStopsProgress) {
    GuardedAsyncScope scope;
    auto contract = folly::coro::makePromiseContract<folly::Unit>();
    scope.add(onWorker(
        folly::coro::co_invoke(
            [future = std::move(contract.second)]() mutable
                -> folly::coro::Task<void> { co_await std::move(future); })));

    std::vector<folly::coro::Task<void>> stops;
    stops.push_back(
        folly::coro::co_invoke([&scope]() -> folly::coro::Task<void> {
            co_await scope.closeAsync();
        }));
    stops.push_back(
        folly::coro::co_invoke(
            [promise = std::move(
                 contract.first)]() mutable -> folly::coro::Task<void> {
                promise.setValue(folly::Unit{});
                co_return;
            }));
    blockingWait(folly::coro::collectAllRange(std::move(stops)));
}
