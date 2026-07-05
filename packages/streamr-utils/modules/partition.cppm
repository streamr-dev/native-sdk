// Module streamr.utils.partition
// CONSOLIDATED from the former header
// streamr-utils/partition.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

export module streamr.utils.partition;

export namespace streamr::utils {

inline constexpr auto MAX_PARTITION_COUNT = 100; // NOLINT

inline void ensureValidStreamPartitionIndex(
    std::optional<uint32_t> streamPartition) {
    if (!streamPartition.has_value() ||
        streamPartition.value() >= MAX_PARTITION_COUNT) {
        throw std::invalid_argument(
            "invalid streamPartition value: " +
            std::to_string(streamPartition.value()));
    }
}

inline void ensureValidStreamPartitionCount(
    std::optional<uint32_t> streamPartition) {
    if (!streamPartition.has_value() ||
        streamPartition.value() > MAX_PARTITION_COUNT) {
        throw std::invalid_argument(
            "invalid streamPartition value: " +
            std::to_string(streamPartition.value()));
    }
}

} // namespace streamr::utils
