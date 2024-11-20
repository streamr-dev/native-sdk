#ifndef STREAMR_UTILS_RUN_AND_WAIT_FOR_EVENTS_HPP
#define STREAMR_UTILS_RUN_AND_WAIT_FOR_EVENTS_HPP

#include <folly/experimental/coro/BlockingWait.h>
#include <folly/experimental/coro/Collect.h>
#include <folly/experimental/coro/Timeout.h>
#include "streamr-utils/ReplayEventEmitterWrapper.hpp"
#include "streamr-utils/waitForEvent.hpp"

namespace streamr::utils {

using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::HandlerToken;
using streamr::utils::waitForEvent;

constexpr std::chrono::milliseconds runAndWaitForEventsDefaultTimeout =
    std::chrono::milliseconds(5000);

template <typename... BoundEventTypes>
inline void runAndWaitForEvents(
    const std::vector<std::function<void()>>& operations,
    const std::tuple<BoundEventTypes...>& eventsToWaitFor,
    std::chrono::milliseconds timeout = runAndWaitForEventsDefaultTimeout) {
    auto replayEventEmitterWrappers = std::apply(
        [](auto&&... args) {
            return std::make_tuple(
                makeReplayEventEmitterWrapper(args.getEmitter())...);
        },
        eventsToWaitFor);

    std::vector<folly::coro::Task<void>> operationTasks;
    for (const auto& operation : operations) {
        operationTasks.push_back(
            folly::coro::co_invoke([operation]() -> folly::coro::Task<void> {
                operation();
                co_return;
            }));
    }

    std::apply(
        [timeout, &operationTasks](auto&... eventEmitterWrapper) {
            folly::coro::blockingWait(folly::coro::timeout(
                folly::coro::collectAll(
                    folly::coro::collectAllRange(std::move(operationTasks)),
                    waitForEvent<typename BoundEventTypes::EventType>(
                        eventEmitterWrapper.get(), timeout)...),
                timeout));
        },
        replayEventEmitterWrappers);
}

} // namespace streamr::utils

#endif // STREAMR_UTILS_RUN_AND_WAIT_FOR_EVENTS_HPP
