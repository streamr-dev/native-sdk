// Module streamr.utils.collect
// CONSOLIDATED from the former header
// streamr-utils/collect.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <tuple>
#include <type_traits>
#include <utility>
#include <folly/experimental/coro/Collect.h>
#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/ViaIfAsync.h>

export module streamr.utils.collect;

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