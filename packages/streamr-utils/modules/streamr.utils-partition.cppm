// Façade partition over streamr-utils/partition.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/partition.hpp"

export module streamr.utils:partition;

export namespace streamr::utils {

using streamr::utils::ensureValidStreamPartitionCount;
using streamr::utils::ensureValidStreamPartitionIndex;
using streamr::utils::MAX_PARTITION_COUNT;

} // namespace streamr::utils
