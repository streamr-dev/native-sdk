#ifndef STREAMR_UTILS_WAITFORCONDITION_HPP
#define STREAMR_UTILS_WAITFORCONDITION_HPP

#include <chrono>
#include <folly/coro/Task.h>
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-utils/AbortController.hpp"
#include "streamr-utils/AbortableTimers.hpp"
#include "streamr-utils/waitForEvent.hpp"
#include <folly/coro/Collect.h>

namespace streamr::utils {

using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

constexpr auto defaultRetryInterval = std::chrono::milliseconds(100);

struct ConditionMet : public Event<> {};

using PollerEvents = std::tuple<ConditionMet>;

class Poller : public EventEmitter<PollerEvents> {
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
                    std::cout << "!!!EMIT";
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
    co_await folly::coro::collectAll(
        waitForEvent<ConditionMet>(poller, timeout, abortSignal), // NOLINT
        folly::coro::co_invoke([&poller, &conditionFn, retryInterval]() -> folly::coro::Task<void> {                   
            poller.start(conditionFn, retryInterval);
            co_return;
        }));
}

} // namespace streamr::utils

#endif // STREAMR_UTILS_WAITFORCONDITION_HPP