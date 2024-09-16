#ifndef STREAMR_UTILS_COLLECT_HPP
#define STREAMR_UTILS_COLLECT_HPP

#include <type_traits>
#include <folly/coro/Collect.h>
#include <folly/coro/Task.h>
#include <folly/coro/ViaIfAsync.h>
#include "streamr-utils/toCoroTask.hpp"

namespace streamr::utils {

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

#endif // STREAMR_UTILS_COLLECT_HPP
