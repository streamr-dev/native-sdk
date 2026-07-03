// Façade partition over streamr-utils/waitForCondition.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/waitForCondition.hpp"

export module streamr.utils:waitForCondition;

export namespace streamr::utils {

using streamr::utils::ConditionMet;
using streamr::utils::defaultRetryInterval;
using streamr::utils::Poller;
using streamr::utils::PollerEvents;
using streamr::utils::waitForCondition;

} // namespace streamr::utils
