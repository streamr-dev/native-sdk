#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.utils.collect;

using streamr::utils::collect;
using streamr::utils::toCoroTask;

TEST(CollectTest, Basic) {
    EXPECT_EQ(
        std::get<0>(streamr::utils::blockingWait(
            collect(toCoroTask([]() { return 42; })))),
        42); // NOLINT
}

TEST(CollectTest, TaskReturningVoid) {
    streamr::utils::blockingWait(collect(toCoroTask([]() { return; })));
}

TEST(CollectTest, FunctionReturningVoid) {
    streamr::utils::blockingWait(collect([]() { return; }));
}

TEST(CollectTest, TaskAndFunctionReturningVoid) {
    streamr::utils::blockingWait(
        collect(toCoroTask([]() { return; }), []() { return; }));
}

TEST(CollectTest, FunctionOnly) {
    EXPECT_EQ(
        std::get<0>(streamr::utils::blockingWait(collect([]() { return 42; }))),
        42); // NOLINT
}

TEST(CollectTest, MixOfTasksAndFunctions) {
    EXPECT_EQ(
        std::get<0>(streamr::utils::blockingWait(
            collect(toCoroTask([]() { return 42; }), []() { return 43; }))),
        42); // NOLINT
}

TEST(CollectTest, MixOfTasksAndFunctionsAndFollySemiFutures) {
    auto [promise, future] = folly::coro::makePromiseContract<int>();
    promise.setValue(42); // NOLINT
    EXPECT_EQ(
        std::get<0>(streamr::utils::blockingWait(collect(
            toCoroTask([]() { return 42; }),
            []() { return 43; },
            std::move(future)))),
        42); // NOLINT
}
