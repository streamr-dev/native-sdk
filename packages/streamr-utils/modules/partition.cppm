// Module streamr.utils.partition
// CONSOLIDATED from the former header
// streamr-utils/partition.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;


export module streamr.utils.partition;

import std;

export namespace streamr::utils {

inline constexpr auto MAX_PARTITION_COUNT = 100; // NOLINT

inline void ensureValidStreamPartitionIndex(
    std::optional<std::uint32_t> streamPartition) {
    if (!streamPartition.has_value() ||
        streamPartition.value() >= MAX_PARTITION_COUNT) {
        throw std::invalid_argument(
            "invalid streamPartition value: " +
            std::to_string(streamPartition.value()));
    }
}

inline void ensureValidStreamPartitionCount(
    std::optional<std::uint32_t> streamPartition) {
    if (!streamPartition.has_value() ||
        streamPartition.value() > MAX_PARTITION_COUNT) {
        throw std::invalid_argument(
            "invalid streamPartition value: " +
            std::to_string(streamPartition.value()));
    }
}

} // namespace streamr::utils
