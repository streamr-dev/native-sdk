/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <thread>

#include <folly/experimental/coro/DetachOnCancel.h>
#include <folly/experimental/coro/Promise.h>
#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/Timeout.h>
#include <folly/experimental/coro/blockingWait.h>

using namespace std::literals::chrono_literals;

folly::coro::Task<> demoDetachOnCancel() {
    co_await folly::coro::co_awaitTry(folly::coro::timeout(
        folly::coro::co_invoke([]() -> folly::coro::Task<> {
            auto&& promiseFuture = folly::coro::makePromiseContract<void>();

            std::thread longTask(
                [promise = std::move(promiseFuture.first)]() mutable {
                    // NOLINTNEXTLINE(facebook-hte-BadCall-sleep_for)
                    std::this_thread::sleep_for(1s);
                    std::cout << "Long running task finishes"
                              << "\n";
                    promise.setValue();
                });

            try {
                co_await folly::coro::detachOnCancel(
                    std::move(promiseFuture.second));
                std::cout << "DONE"
                          << "\n";
            } catch (folly::OperationCancelled&) {
                std::cout << "cancelled"
                          << "\n";
            }
            longTask.join();
        }),
        1s));
    co_return;
}

int main() {
    folly::coro::blockingWait(demoDetachOnCancel());
    return 0;
}
