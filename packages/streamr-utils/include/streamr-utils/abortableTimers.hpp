#ifndef STREAMR_UTILS_ABORTABLE_TIMERS_HPP
#define STREAMR_UTILS_ABORTABLE_TIMERS_HPP

namespace streamr::utils {
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>

void setAbortableTimeout(std::function<void()> callback, std::chrono::milliseconds timeout, std::shared_ptr<AbortSignal> signal) {
    
}

} // namespace streamr::utils

#endif // STREAMR_UTILS_ABORTABLE_TIMERS_HPP