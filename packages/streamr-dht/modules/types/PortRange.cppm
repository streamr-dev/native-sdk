// Module streamr.dht.PortRange
// CONSOLIDATED from the former header streamr-dht/types/PortRange.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;


export module streamr.dht.PortRange;

import std;
export namespace streamr::dht::types {

struct PortRange {
    std::uint16_t min;
    std::uint16_t max;
};

} // namespace streamr::dht::types
