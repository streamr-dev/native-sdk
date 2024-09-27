#ifndef STREAMR_UTILS_WAITFOREVENT_HPP
#define STREAMR_UTILS_WAITFOREVENT_HPP

#include <chrono>
#include <functional>
#include <tuple>
#include <folly/experimental/coro/Promise.h>
#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/Timeout.h>
#include "streamr-utils/AbortController.hpp"
namespace streamr::utils {

template <typename T>
struct remove_pointer { // NOLINT
    using type = T;
};

template <typename T>
struct remove_pointer<T*> {
    using type = typename remove_pointer<T>::type;
};

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
inline folly::coro::Task<
    typename remove_pointer<EventType>::type::ArgumentTypes>
waitForEvent(
    EmitterType* emitter,
    std::chrono::milliseconds timeout = defaultTimeout,
    AbortSignal* abortSignal = nullptr) {
    Waiter<typename remove_pointer<EventType>::type::ArgumentTypes> waiter;
    emitter->template once<typename remove_pointer<EventType>::type>(
        waiter.function);
    if (abortSignal) {
        co_return co_await folly::coro::co_withCancellation(
            abortSignal->getCancellationToken(),
            folly::coro::timeout(
                std::move(waiter.promiseContract.second), timeout));
    }
    co_return co_await folly::coro::timeout(
        std::move(waiter.promiseContract.second), timeout);
}

} // namespace streamr::utils
#endif