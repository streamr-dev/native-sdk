#include "streamr-utils/toCoroTask.hpp"
#include <gtest/gtest.h>
#include <folly/coro/BlockingWait.h>
#include <folly/coro/Task.h>

using streamr::utils::toCoroTask;

TEST(ToCoroTaskTest, Basic) {
    EXPECT_EQ(
        folly::coro::blockingWait(toCoroTask([]() { return 42; })),
        42); // NOLINT
}