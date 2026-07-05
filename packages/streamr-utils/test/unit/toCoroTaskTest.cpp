#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.utils.toCoroTask;

using streamr::utils::toCoroTask;

TEST(ToCoroTaskTest, Basic) {
    EXPECT_EQ(
        streamr::utils::blockingWait(toCoroTask([]() { return 42; })),
        42); // NOLINT
}