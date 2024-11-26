#include "streamr-utils/AbortableTimers.hpp"
#include <future>
#include <gtest/gtest.h>
#include "streamr-utils/AbortController.hpp"

using streamr::utils::AbortableTimers;
using streamr::utils::AbortController;

TEST(AbortableTimers, CallbackCalled) {
    AbortController controller;
    std::promise<std::string> promise;
    auto future = promise.get_future();
    AbortableTimers::setAbortableTimeout(
        [&promise]() { promise.set_value("test"); },
        std::chrono::milliseconds(100), // NOLINT
        controller.getSignal());
    EXPECT_EQ(future.get(), "test");
}

TEST(AbortableTimers, CallingAbotAfterTimeout) {
    AbortController controller;
    std::promise<std::string> promise;
    auto future = promise.get_future();
    AbortableTimers::setAbortableTimeout(
        [&promise]() { promise.set_value("test"); },
        std::chrono::milliseconds(100), // NOLINT
        controller.getSignal());
    EXPECT_EQ(future.get(), "test");
    controller.abort("test");
    EXPECT_TRUE(controller.getSignal().aborted);
    EXPECT_EQ(controller.getSignal().reason, "test");
}

TEST(AbortableTimers, Abort) {
    AbortController controller;
    AbortableTimers::setAbortableTimeout(
        []() { FAIL() << "Callback should not be called"; },
        std::chrono::milliseconds(100), // NOLINT
        controller.getSignal());
    controller.abort("test");
    EXPECT_TRUE(controller.getSignal().aborted);
    EXPECT_EQ(controller.getSignal().reason, "test");
}

// Test disabled because it does not guaranteed to work on slow runners

TEST(AbortableTimers, DISABLED_Interval) {
    AbortController controller;
    int counter = 0;
    AbortableTimers::setAbortableInterval(
        [&counter]() { counter++; },
        std::chrono::milliseconds(100), // NOLINT
        controller.getSignal());
    std::this_thread::sleep_for(std::chrono::milliseconds(550)); // NOLINT
    controller.abort("test");
    EXPECT_EQ(counter, 6);
}