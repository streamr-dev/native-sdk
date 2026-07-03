// Façade partition over streamr-utils/StreamPartID.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/StreamPartID.hpp"

export module streamr.utils:StreamPartID;

export namespace streamr::utils {

using streamr::utils::DELIMITER;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::toStreamPartID;

} // namespace streamr::utils
