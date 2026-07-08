// Module streamr.utils.waitForCondition
// CONSOLIDATED from the former header
// streamr-utils/waitForCondition.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.


export module streamr.utils.waitForCondition;

import std;

import streamr.utils.CoroutineHelper;
import streamr.eventemitter.EventEmitter;
import streamr.utils.AbortController;
import streamr.utils.AbortableTimers;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.waitForEvent;

export namespace streamr::utils {

using streamr::eventemitter::Event;
using streamr::eventemitter::ReplayEventEmitter;

inline constexpr auto defaultRetryInterval =
    std::chrono::milliseconds(100); // NOLINT

struct ConditionMet : public Event<> {};

using PollerEvents = std::tuple<ConditionMet>;

// The poll callback runs on the global AbortableTimers scheduler thread
// and one in-flight invocation can outlive abort() (FunctionScheduler
// cancellation does not interrupt a running function). The callback
// therefore keeps the Poller alive through a shared_ptr self-capture and
// re-checks the abort signal, so a Poller can never be used after
// destruction. (The former stack-allocated Poller with a raw `this`
// capture segfaulted under the simulator-based integration tests,
// phase 0.3.)
class Poller : public ReplayEventEmitter<PollerEvents>,
               public EnableSharedFromThis {
private:
    struct Private {
        explicit Private() = default;
    };

    AbortController abortController;

public:
    explicit Poller(Private /*enforces newInstance*/) {}

    [[nodiscard]] static std::shared_ptr<Poller> newInstance() {
        return std::make_shared<Poller>(Private{});
    }

    void start(
        std::function<bool()> conditionFn,
        std::chrono::milliseconds retryInterval) {
        auto self = this->sharedFromThis<Poller>();
        AbortableTimers::setAbortableInterval(
            [self, fn = std::move(conditionFn)]() {
                if (self->abortController.getSignal().aborted) {
                    // stopped between scheduling and this invocation
                    return;
                }
                if (fn()) {
                    self->stop();
                    self->emit<ConditionMet>();
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
    auto poller = Poller::newInstance();
    poller->start(std::move(conditionFn), retryInterval);
    // Stop polling on EVERY path — the previous implementation never
    // stopped the poller on the timeout path, leaving the interval
    // firing forever. removeAllListeners() then synchronizes with any
    // in-flight emit (emit holds the emitter lock while invoking
    // handlers), so the waiter in this coroutine frame cannot be touched
    // after the frame unwinds.
    try {
        co_await waitForEvent<ConditionMet>(poller.get(), timeout, abortSignal);
    } catch (...) {
        poller->stop();
        poller->removeAllListeners();
        throw;
    }
    poller->stop();
    poller->removeAllListeners();
}

} // namespace streamr::utils