// Façade partition over streamr-utils/StreamID.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/StreamID.hpp"

export module streamr.utils:StreamID;

export namespace streamr::utils {

using streamr::utils::StreamID;
using streamr::utils::StreamIDUtils;
using streamr::utils::toStreamID;

} // namespace streamr::utils
