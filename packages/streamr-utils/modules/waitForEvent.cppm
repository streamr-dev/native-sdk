// Module streamr.utils.waitForEvent
// CONSOLIDATED from the former header
// streamr-utils/waitForEvent.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.


export module streamr.utils.waitForEvent;

import std;

import streamr.utils.CoroutineHelper;
import streamr.utils.AbortController;
import streamr.eventemitter.EventEmitter;

export namespace streamr::utils {

using streamr::eventemitter::HandlerToken;

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
    using RealEvent = typename remove_pointer<EventType>::type;
    // The waiter is heap-owned and CO-OWNED by the registered listener,
    // so a delivery that races the timeout or cancellation operates on a
    // live promise. The old code registered a listener capturing a stack
    // waiter and never removed it on the timeout path: once the coroutine
    // frame unwound, a later emit invoked the dangling listener and
    // called setValue on the destroyed promise (folly Promise.h
    // "Check failed: state_" abort, hit in the connection-teardown stress
    // runs, phase A0).
    auto waiter = std::make_shared<Waiter<typename RealEvent::ArgumentTypes>>();
    auto token = emitter->template once<RealEvent>([waiter](auto&&... args) {
        waiter->function(std::forward<decltype(args)>(args)...);
    });
    // Remove the listener on every exit path. When the event fired, once()
    // already removed it and this off() is a no-op; on the timeout and
    // cancellation paths this is what unregisters it. A listener
    // invocation already in flight on another thread keeps the waiter
    // alive through its own copy of the callable, so this never races the
    // waiter's destruction. The remover is declared after the waiter so
    // it is destroyed (and unregisters) before the waiter shared_ptr in
    // this frame drops.
    struct Remover {
        EmitterType* emitter;
        HandlerToken token;
        ~Remover() { emitter->template off<RealEvent>(token); }
    } remover{emitter, token};
    if (abortSignal) {
        co_return co_await streamr::utils::co_withCancellation(
            abortSignal->getCancellationToken(),
            folly::coro::timeout(
                std::move(waiter->promiseContract.second), timeout));
    }
    co_return co_await folly::coro::timeout(
        std::move(waiter->promiseContract.second), timeout);
}

} // namespace streamr::utils