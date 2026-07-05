// Module streamr.utils.toCoroTask
// CONSOLIDATED from the former header
// streamr-utils/toCoroTask.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <type_traits>
#include <folly/experimental/coro/Task.h>

export module streamr.utils.toCoroTask;

export namespace streamr::utils {

template <typename F>
inline auto toCoroTask(F&& fn) -> folly::coro::Task<std::invoke_result_t<F>> {
    return folly::coro::co_invoke(
        [fn]() -> folly::coro::Task<std::invoke_result_t<F>> {
            co_return fn();
        });
}

} // namespace streamr::utils