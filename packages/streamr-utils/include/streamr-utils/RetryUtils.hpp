#ifndef STREAMR_UTILS_RETRY_UTILS_HPP
#define STREAMR_UTILS_RETRY_UTILS_HPP

#include <string>
#include <folly/coro/Promise.h>
#include <folly/coro/Sleep.h>
#include <folly/coro/Task.h>
#include <folly/futures/Future.h>
#include <folly/futures/Promise.h>
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/AbortController.hpp"
namespace streamr::utils {

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
                co_await folly::coro::co_withCancellation(
                    std::move(cancelToken), folly::coro::sleep(delay));
            } catch (const folly::FutureCancellation&) {
                break;
            }
        }
    }
};
} // namespace streamr::utils

#endif // STREAMR_UTILS_RETRY_UTILS_HPP