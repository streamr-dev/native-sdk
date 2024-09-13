#ifndef STREAMR_UTILS_PARTITION_HPP
#define STREAMR_UTILS_PARTITION_HPP

#include <cstdint>
#include <optional>
#include <string>

namespace streamr::utils {

constexpr auto MAX_PARTITION_COUNT = 100; //NOLINT

inline void ensureValidStreamPartitionIndex(std::optional<uint32_t> streamPartition) {
    if (!streamPartition.has_value() || streamPartition.value() >= MAX_PARTITION_COUNT) {
        throw std::invalid_argument("invalid streamPartition value: " + std::to_string(streamPartition.value()));
    }
}

inline void ensureValidStreamPartitionCount(std::optional<uint32_t> streamPartition) {
    if (!streamPartition.has_value() || streamPartition.value() > MAX_PARTITION_COUNT) {
        throw std::invalid_argument("invalid streamPartition value: " + std::to_string(streamPartition.value()));
    }
}


} // namespace streamr::utils

#endif // STREAMR_UTILS_PARTITION_HPP