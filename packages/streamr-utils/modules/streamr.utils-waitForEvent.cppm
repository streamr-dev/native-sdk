// Façade partition over streamr-utils/waitForEvent.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/waitForEvent.hpp"

export module streamr.utils:waitForEvent;

export namespace streamr::utils {

using streamr::utils::defaultTimeout;
using streamr::utils::remove_pointer;
using streamr::utils::Waiter;
using streamr::utils::waitForEvent;

} // namespace streamr::utils
