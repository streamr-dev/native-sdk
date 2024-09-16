#include "streamr-utils/collect.hpp"
#include <gtest/gtest.h>
#include <folly/coro/BlockingWait.h>
#include <folly/coro/Promise.h>
#include <folly/coro/Task.h>

using streamr::utils::collect;
using streamr::utils::toCoroTask;

TEST(CollectTest, Basic) {
    EXPECT_EQ(
        std::get<0>(folly::coro::blockingWait(
            collect(toCoroTask([]() { return 42; })))),
        42); // NOLINT
}

TEST(CollectTest, TaskReturningVoid) {
    folly::coro::blockingWait(collect(toCoroTask([]() { return; })));
}

TEST(CollectTest, FunctionReturningVoid) {
    folly::coro::blockingWait(collect([]() { return; }));
}

TEST(CollectTest, TaskAndFunctionReturningVoid) {
    folly::coro::blockingWait(
        collect(toCoroTask([]() { return; }), []() { return; }));
}

TEST(CollectTest, FunctionOnly) {
    EXPECT_EQ(
        std::get<0>(folly::coro::blockingWait(collect([]() { return 42; }))),
        42); // NOLINT
}

TEST(CollectTest, MixOfTasksAndFunctions) {
    EXPECT_EQ(
        std::get<0>(folly::coro::blockingWait(
            collect(toCoroTask([]() { return 42; }), []() { return 43; }))),
        42); // NOLINT
}

TEST(CollectTest, MixOfTasksAndFunctionsAndFollySemiFutures) {
    auto [promise, future] = folly::coro::makePromiseContract<int>();
    promise.setValue(42); // NOLINT
    EXPECT_EQ(
        std::get<0>(folly::coro::blockingWait(collect(
            toCoroTask([]() { return 42; }),
            []() { return 43; },
            std::move(future)))),
        42); // NOLINT
}
