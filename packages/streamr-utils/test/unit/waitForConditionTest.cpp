#include "streamr-utils/waitForCondition.hpp"
#include <gtest/gtest.h>
#include <folly/experimental/coro/BlockingWait.h>

using streamr::utils::AbortController;
using streamr::utils::waitForCondition;
using namespace std::chrono_literals;

class WaitForConditionTest : public ::testing::Test {
protected:
    AbortController abortController; // NOLINT
};

TEST_F(WaitForConditionTest, ConditionMetImmediately) {
    bool conditionMet = false;
    std::function<bool()> condition = [&conditionMet]() {
        conditionMet = true;
        return conditionMet;
    };
    auto task = waitForCondition(std::move(condition));
    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
}

TEST_F(WaitForConditionTest, ConditionMetAfterDelay) {
    bool conditionMet = false;
    auto start = std::chrono::steady_clock::now();

    auto task = waitForCondition(
        [&]() {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= 500ms) { // NOLINT
                conditionMet = true;
            }
            return conditionMet;
        },
        5s, // NOLINT
        100ms // NOLINT
    );

    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    EXPECT_TRUE(conditionMet);
}

TEST_F(WaitForConditionTest, TimeoutExceeded) {
    auto task =
        waitForCondition([]() { return false; }, 500ms, 100ms); // NOLINT
    EXPECT_THROW(
        folly::coro::blockingWait(std::move(task)), folly::FutureTimeout);
}

TEST_F(WaitForConditionTest, AbortSignalTriggered) {
    bool conditionMet = false;
    auto task = waitForCondition(
        [&]() { return conditionMet; },
        5s, // NOLINT
        100ms, // NOLINT
        &abortController.getSignal());

    // Abort the operation after a short delay
    std::thread([this]() {
        std::this_thread::sleep_for(200ms); // NOLINT
        abortController.abort();
    }).detach();

    EXPECT_THROW(
        folly::coro::blockingWait(std::move(task)), folly::OperationCancelled);
}

// Test disabled because it does not guaranteed to work on slow runners
TEST_F(WaitForConditionTest, DISABLED_CustomRetryInterval) {
    int callCount = 0;
    auto start = std::chrono::steady_clock::now();

    auto task = waitForCondition(
        [&]() {
            callCount++;
            return callCount >= 3;
        },
        5s, // NOLINT
        200ms // NOLINT
    );

    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, 400ms);
    EXPECT_LT(elapsed, 600ms);
    EXPECT_EQ(callCount, 3);
}
