#ifndef STREAMR_UTILS_WAITFORCONDITION_HPP
#define STREAMR_UTILS_WAITFORCONDITION_HPP

#include <chrono>
#include <folly/experimental/coro/Collect.h>
#include <folly/coro/Task.h>
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-utils/AbortController.hpp"
#include "streamr-utils/AbortableTimers.hpp"
#include "streamr-utils/waitForEvent.hpp"

namespace streamr::utils {

using streamr::eventemitter::Event;
using streamr::eventemitter::ReplayEventEmitter;

constexpr auto defaultRetryInterval = std::chrono::milliseconds(100); // NOLINT

struct ConditionMet : public Event<> {};

using PollerEvents = std::tuple<ConditionMet>;

class Poller : public ReplayEventEmitter<PollerEvents> {
private:
    AbortController abortController;

public:
    void start(
        const std::function<bool()>& conditionFn,
        std::chrono::milliseconds retryInterval) {
        AbortableTimers::setAbortableInterval(
            [this, conditionFn]() {
                if (conditionFn()) {
                    this->stop();
                    this->emit<ConditionMet>();
                }
            },
            retryInterval,
            this->abortController.getSignal());
    }

    void stop() { abortController.abort(); }
};

inline folly::coro::Task<void> waitForCondition(
    const std::function<bool()>& conditionFn,
    std::chrono::milliseconds timeout = defaultTimeout, // NOLINT
    std::chrono::milliseconds retryInterval = defaultRetryInterval,
    AbortSignal* abortSignal = nullptr) {
    Poller poller;
    poller.start(conditionFn, retryInterval);
    co_await waitForEvent<ConditionMet>(&poller, timeout, abortSignal);
}

} // namespace streamr::utils

#endif // STREAMR_UTILS_WAITFORCONDITION_HPP