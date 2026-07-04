// Module streamr.dht.PortRange
// CONSOLIDATED from the former header streamr-dht/types/PortRange.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <cstdint>

export module streamr.dht.PortRange;
export namespace streamr::dht::types {

struct PortRange {
    uint16_t min;
    uint16_t max;
};

} // namespace streamr::dht::types
