#include <gtest/gtest.h>
#include <streamr-utils/waitForEvent.hpp>
#include <streamr-utils/AbortController.hpp>
#include <streamr-eventemitter/EventEmitter.hpp>
#include <folly/coro/Invoke.h>
#include <folly/coro/BlockingWait.h>
#include <folly/coro/Collect.h>
#include <folly/coro/Sleep.h>
#include <folly/coro/Timeout.h>
#include <tuple>

using streamr::utils::waitForEvent;
using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::Event;
using streamr::utils::AbortController;
using namespace std::chrono_literals;

struct Connected : public Event<> {};
struct Disconnected : public Event<std::string /*reason*/> {};
using TestEvents = std::tuple<Connected,Disconnected>;


TEST(WaitForEventTest, WaitForVoidEvent) {
    EventEmitter<TestEvents> emitter;
   
    folly::coro::blockingWait(
        folly::coro::co_invoke([&emitter]() -> folly::coro::Task<void>{
            auto result = co_await folly::coro::collectAll(
                waitForEvent<Connected>(emitter), // NOLINT
                folly::coro::co_invoke([&emitter]() -> folly::coro::Task<void> {
                    emitter.emit<Connected>();
                    co_return;
                })  
            );
        })
    );
}

TEST(WaitForEventTest, WaitForStringEvent) {
    EventEmitter<TestEvents> emitter;
    
    folly::coro::blockingWait(
        folly::coro::co_invoke([&emitter]() -> folly::coro::Task<void>{
            auto result =co_await folly::coro::collectAll(
                waitForEvent<Disconnected>(emitter), // NOLINT
                folly::coro::co_invoke([&emitter]() -> folly::coro::Task<void> {
                    emitter.emit<Disconnected>("test");
                    co_return;
                })
            );
            EXPECT_EQ(std::get<0>(std::get<0>(result)), "test");
        })
    );
}

TEST(WaitForEventTest, WaitForTimeout) {
   EventEmitter<TestEvents> emitter;
    
    EXPECT_THROW(
    folly::coro::blockingWait(
        folly::coro::co_invoke([&emitter]() -> folly::coro::Task<void>{
            auto result = co_await folly::coro::collectAll(
                waitForEvent<Disconnected>(emitter, 10ms), // NOLINT
                folly::coro::co_invoke([&emitter]() -> folly::coro::Task<void> {
                    co_await folly::coro::sleep(1s);   // NOLINT
                    emitter.emit<Disconnected>("test");
                    co_return;
                })
            );
        })
    ), folly::FutureTimeout);
}

TEST(WaitForEventTest, WaitForStringEventWithAbortSignal) {
    EventEmitter<TestEvents> emitter;
    AbortController abortController;

    EXPECT_THROW(
    folly::coro::blockingWait(
        folly::coro::co_invoke([&emitter, &abortController]() -> folly::coro::Task<void>{
            auto result =co_await folly::coro::collectAll(
                waitForEvent<Disconnected>(emitter, 1000ms, &(abortController.signal)), // NOLINT
                folly::coro::co_invoke([&abortController]() -> folly::coro::Task<void> {
                    abortController.abort();
                    co_return;
                })
            );
        })
    ), folly::OperationCancelled);
}

