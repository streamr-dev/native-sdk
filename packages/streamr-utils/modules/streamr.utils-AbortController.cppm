// Façade partition over streamr-utils/AbortController.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/AbortController.hpp"

export module streamr.utils:AbortController;

export namespace streamr::utils::abortsignalevents {

using streamr::utils::abortsignalevents::Aborted;

} // namespace streamr::utils::abortsignalevents

export namespace streamr::utils {

using streamr::utils::AbortController;
using streamr::utils::AbortSignal;
using streamr::utils::AbortSignalEvents;
using streamr::utils::abortsignalevents::Aborted;

} // namespace streamr::utils
