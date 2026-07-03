// Façade partition over streamr-utils/ReplayEventEmitterWrapper.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/ReplayEventEmitterWrapper.hpp"

export module streamr.utils:ReplayEventEmitterWrapper;

export namespace streamr::utils {

using streamr::utils::createReplayEventEmitterWrapper;
using streamr::utils::makeReplayEventEmitterWrapper;
using streamr::utils::ReplayEventEmitterWrapper;

} // namespace streamr::utils
