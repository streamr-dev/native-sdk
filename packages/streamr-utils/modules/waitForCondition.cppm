// Module streamr.utils.waitForCondition
// CONSOLIDATED from the former header
// streamr-utils/waitForCondition.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <chrono>
#include <functional>
#include <tuple>
#include <utility>
#include <folly/coro/Task.h>
#include <folly/experimental/coro/Collect.h>

export module streamr.utils.waitForCondition;

import streamr.eventemitter.EventEmitter;
import streamr.utils.AbortController;
import streamr.utils.AbortableTimers;
import streamr.utils.waitForEvent;

export namespace streamr::utils {

using streamr::eventemitter::Event;
using streamr::eventemitter::ReplayEventEmitter;

inline constexpr auto defaultRetryInterval =
    std::chrono::milliseconds(100); // NOLINT

struct ConditionMet : public Event<> {};

using PollerEvents = std::tuple<ConditionMet>;

class Poller : public ReplayEventEmitter<PollerEvents> {
private:
    AbortController abortController;

public:
    void start(
        std::function<bool()> conditionFn,
        std::chrono::milliseconds retryInterval) {
        AbortableTimers::setAbortableInterval(
            [this, fn = std::move(conditionFn)]() {
                if (fn()) {
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
    std::function<bool()> conditionFn,
    std::chrono::milliseconds timeout = defaultTimeout, // NOLINT
    std::chrono::milliseconds retryInterval = defaultRetryInterval,
    AbortSignal* abortSignal = nullptr) {
    Poller poller;
    poller.start(std::move(conditionFn), retryInterval);
    co_await waitForEvent<ConditionMet>(&poller, timeout, abortSignal);
}

} // namespace streamr::utils