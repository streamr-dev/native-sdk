// Module streamr.utils.waitForEvent
// CONSOLIDATED from the former header
// streamr-utils/waitForEvent.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <functional>
#include <tuple>
#include <utility>

export module streamr.utils.waitForEvent;

import streamr.utils.CoroutineHelper;
import streamr.utils.AbortController;

export namespace streamr::utils {

template <typename T>
struct remove_pointer { // NOLINT
    using type = T;
};

template <typename T>
struct remove_pointer<T*> {
    using type = typename remove_pointer<T>::type;
};

inline constexpr auto defaultTimeout = std::chrono::milliseconds(5000);

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
        co_return co_await streamr::utils::co_withCancellation(
            abortSignal->getCancellationToken(),
            folly::coro::timeout(
                std::move(waiter.promiseContract.second), timeout));
    }
    co_return co_await folly::coro::timeout(
        std::move(waiter.promiseContract.second), timeout);
}

} // namespace streamr::utils