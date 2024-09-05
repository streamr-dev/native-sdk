#ifndef STREAMR_UTILS_WAITFOREVENT_HPP
#define STREAMR_UTILS_WAITFOREVENT_HPP

#include <chrono>
#include <functional>
#include <memory>
#include <tuple>
#include <folly/coro/Promise.h>
#include <folly/coro/Task.h>
#include <folly/coro/Timeout.h>
#include "streamr-utils/AbortController.hpp"

namespace streamr::utils {

constexpr auto defaultTimeout = std::chrono::milliseconds(5000);

// Disable default specialization
template <typename T>
struct Waiter;

// Specialization for std::tuple<Args...>
template <typename... Args>
struct Waiter<std::tuple<Args...>> {
    using FunctionType = std::function<void(Args...)>;

    std::pair<
        folly::coro::Promise<std::tuple<Args...>>,
        folly::coro::Future<std::tuple<Args...>>>
        promiseContract =
            folly::coro::makePromiseContract<std::tuple<Args...>>();

    FunctionType function = [this](Args... args) mutable {
        promiseContract.first.setValue(
            std::move(std::tuple<Args...>(std::forward<Args>(args)...)));
    };
};

template <typename EventType, typename EmitterType>
inline folly::coro::Task<typename EventType::ArgumentTypes> waitForEvent(
    EmitterType& emitter,
    std::chrono::milliseconds timeout = defaultTimeout,
    AbortSignal* abortSignal = nullptr) {
    Waiter<typename EventType::ArgumentTypes> waiter;
    emitter.template once<EventType>(waiter.function);
    if (abortSignal) {
        co_return co_await folly::coro::co_withCancellation(
            abortSignal->getCancellationToken(),
            folly::coro::timeout(std::move(waiter.promiseContract.second), timeout));
    }
    co_return co_await folly::coro::timeout(
        std::move(waiter.promiseContract.second), timeout);
}

} // namespace streamr::utils
#endif