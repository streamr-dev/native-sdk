// Module streamr.utils.runAndWaitForEvents
// CONSOLIDATED from the former header
// streamr-utils/runAndWaitForEvents.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <functional>
#include <tuple>
#include <utility>
#include <vector>

export module streamr.utils.runAndWaitForEvents;

import streamr.utils.CoroutineHelper;
import streamr.eventemitter.EventEmitter;
import streamr.utils.ReplayEventEmitterWrapper;
import streamr.utils.waitForEvent;

export namespace streamr::utils {

using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::HandlerToken;
using streamr::utils::waitForEvent;

inline constexpr std::chrono::milliseconds runAndWaitForEventsDefaultTimeout =
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
            streamr::utils::blockingWait(
                folly::coro::timeout(
                    folly::coro::collectAll(
                        folly::coro::collectAllRange(std::move(operationTasks)),
                        waitForEvent<typename BoundEventTypes::EventType>(
                            eventEmitterWrapper.get(), timeout)...),
                    timeout));
        },
        replayEventEmitterWrappers);
}

} // namespace streamr::utils