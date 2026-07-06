// Module streamr.utils.RetryUtils
// CONSOLIDATED from the former header
// streamr-utils/RetryUtils.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <exception>
#include <functional>
#include <string>
#include <utility>

export module streamr.utils.RetryUtils;

import streamr.utils.CoroutineHelper;
import streamr.logger.SLogger;
import streamr.utils.AbortController;

export namespace streamr::utils {

using streamr::logger::SLogger;

class RetryUtils {
public:
    // NOLINTNEXTLINE
    static constexpr std::chrono::milliseconds DEFAULT_DELAY =
        std::chrono::milliseconds(10000);

    template <typename ReturnType>
    static folly::coro::Task<ReturnType> constantRetry(
        std::function<ReturnType()> task,
        const std::string& description,
        AbortController& abortController,
        std::chrono::milliseconds delay = DEFAULT_DELAY) {
        while (true) {
            try {
                co_return task();
            } catch (const std::exception& e) {
                SLogger::warn(
                    "Failed " + description + ", retrying after " +
                    std::to_string(static_cast<uint64_t>(delay.count())) +
                    "ms delay: " + e.what());
            }
            auto cancelToken =
                abortController.getSignal().getCancellationToken();
            try {
                co_await streamr::utils::co_withCancellation(
                    std::move(cancelToken), folly::coro::sleep(delay));
            } catch (const folly::FutureCancellation&) {
                break;
            }
        }
    }
};
} // namespace streamr::utils