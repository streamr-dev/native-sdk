// Module streamr.utils.collect
// CONSOLIDATED from the former header
// streamr-utils/collect.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;
#include <new>

// std::coroutine_traits must be visible in every translation unit
// that defines OR instantiates a coroutine; it cannot arrive through
// an imported BMI.


export module streamr.utils.collect;

import std;

import streamr.utils.CoroutineHelper;
import streamr.utils.toCoroTask;

export namespace streamr::utils {

using streamr::utils::toCoroTask;

template <typename T>
struct ResultType {
    using type = std::conditional_t<
        std::is_same_v<typename folly::coro::semi_await_result_t<T>, void>,
        folly::Unit,
        typename folly::coro::semi_await_result_t<T>>;
};

// specialization for invocable types
template <typename T>
    requires std::is_invocable_v<T>
struct ResultType<T> {
    using type = std::conditional_t<
        std::is_same_v<std::invoke_result_t<T>, void>,
        folly::Unit,
        std::invoke_result_t<T>>;
};

template <typename... Tasks>
inline auto collect(Tasks&&... tasks)
    -> folly::coro::Task<std::tuple<typename ResultType<Tasks>::type...>> {
    return folly::coro::collectAll(([]<typename T>(T&& t) {
        if constexpr (std::is_invocable_v<T>) {
            return toCoroTask(std::forward<T>(t));
        } else {
            return std::forward<T>(t);
        }
    }(std::forward<Tasks>(tasks)))...);
}

} // namespace streamr::utils