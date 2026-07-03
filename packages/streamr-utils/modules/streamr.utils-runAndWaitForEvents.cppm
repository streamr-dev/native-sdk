// Façade partition over streamr-utils/runAndWaitForEvents.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/runAndWaitForEvents.hpp"

export module streamr.utils:runAndWaitForEvents;

export namespace streamr::utils {

using streamr::utils::runAndWaitForEvents;
using streamr::utils::runAndWaitForEventsDefaultTimeout;

} // namespace streamr::utils
