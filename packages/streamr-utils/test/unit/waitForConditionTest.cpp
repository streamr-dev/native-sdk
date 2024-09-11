#include <gtest/gtest.h>
#include <folly/experimental/coro/BlockingWait.h>
#include "streamr-utils/waitForCondition.hpp"

using namespace streamr::utils;
using namespace std::chrono_literals;

class WaitForConditionTest : public ::testing::Test {
protected:
    AbortController abortController;
};

TEST_F(WaitForConditionTest, ConditionMetImmediately) {
    bool conditionMet = false;
   //    auto task = waitForCondition([&]() { return conditionMet; });
    std::function<bool()> condition = [&conditionMet]() { 
        conditionMet = true;
        return conditionMet;  
    };
    auto task = waitForCondition(condition);
    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
}

TEST_F(WaitForConditionTest, ConditionMetAfterDelay) {
    bool conditionMet = false;
    auto start = std::chrono::steady_clock::now();
    
    auto task = waitForCondition(
        [&]() {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= 500ms) {
                conditionMet = true;
            }
            return conditionMet;
        },
        5s,
        100ms
    );

    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    EXPECT_TRUE(conditionMet);
}

TEST_F(WaitForConditionTest, TimeoutExceeded) {
    auto task = waitForCondition([&]() { return false; }, 500ms, 100ms);
    EXPECT_THROW(folly::coro::blockingWait(std::move(task)), folly::FutureTimeout);
}
/*
TEST_F(WaitForConditionTest, AbortSignalTriggered) {
    bool conditionMet = false;
    auto task = waitForCondition(
        [&]() { return conditionMet; },
        5s,
        100ms,
        &abortController.getSignal()
    );

    // Abort the operation after a short delay
    std::thread([this]() {
        std::this_thread::sleep_for(200ms);
        abortController.abort();
    }).detach();

    EXPECT_THROW(folly::coro::blockingWait(std::move(task)), std::runtime_error);
}
*/

TEST_F(WaitForConditionTest, CustomRetryInterval) {
    int callCount = 0;
    auto start = std::chrono::steady_clock::now();

    auto task = waitForCondition(
        [&]() {
            callCount++;
            return callCount >= 3;
        },
        5s,
        200ms
    );

    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_GE(elapsed, 400ms);
    EXPECT_LT(elapsed, 600ms);
    EXPECT_EQ(callCount, 3);
}


