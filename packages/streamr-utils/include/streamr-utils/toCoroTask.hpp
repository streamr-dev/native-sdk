#ifndef STREAMR_UTILS_TOCOROTASK_HPP
#define STREAMR_UTILS_TOCOROTASK_HPP

#include <folly/coro/Task.h>

namespace streamr::utils {

template <typename F>
inline auto toCoroTask(F&& fn) -> folly::coro::Task<std::invoke_result_t<F>> {
    return folly::coro::co_invoke(
        [fn]() -> folly::coro::Task<std::invoke_result_t<F>> {
            co_return fn();
        });
}

} // namespace streamr::utils
#endif // STREAMR_UTILS_TOCOROTASK_HPP