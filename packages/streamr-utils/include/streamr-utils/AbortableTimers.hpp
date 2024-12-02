#ifndef STREAMR_UTILS_ABORTABLE_TIMERS_HPP
#define STREAMR_UTILS_ABORTABLE_TIMERS_HPP

#include <folly/executors/FunctionScheduler.h>
#include "AbortController.hpp"

namespace streamr::utils {

using streamr::utils::AbortSignal;
using streamr::utils::abortsignalevents::Aborted;
class AbortableTimers {
public:
    static void setAbortableTimeout(
        std::function<void()> callback,
        std::chrono::milliseconds timeout,
        AbortSignal& abortSignal) {
        if (abortSignal.aborted) {
            return;
        }

        const auto signalName = std::to_string(getNextId());
        auto& scheduler = getScheduler();

        auto token = abortSignal.once<Aborted>([&scheduler, signalName]() {
            scheduler.cancelFunction(signalName);
        });

        scheduler.addFunctionOnce(
            [signalName,
             token,
             callback = std::move(callback),
             &abortSignal,
             &scheduler]() {
                if (abortSignal.aborted) {
                    return;
                }
                abortSignal.off<Aborted>(token);
                callback();
            },
            signalName,
            std::chrono::duration_cast<std::chrono::microseconds>(timeout));
    }

    static void setAbortableInterval(
        std::function<void()> callback,
        std::chrono::milliseconds interval,
        AbortSignal& abortSignal) {
        const auto signalName = std::to_string(getNextId());
        auto& scheduler = getScheduler();

        abortSignal.once<Aborted>([&scheduler, signalName]() {
            scheduler.cancelFunction(signalName);
        });

        scheduler.addFunction(
            std::move(callback),
            std::chrono::duration_cast<std::chrono::microseconds>(interval),
            signalName,
            std::chrono::microseconds(0));
    }

private:
    static size_t getNextId() {
        static std::atomic<size_t> id = 0;
        return id.fetch_add(1, std::memory_order_relaxed);
    }

    static folly::FunctionScheduler& getScheduler() {
        // magic static
        static folly::FunctionScheduler scheduler;
        scheduler.start();
        return scheduler;
    }
};

} // namespace streamr::utils
#endif // STREAMR_UTILS_ABORTABLE_TIMERS_HPP