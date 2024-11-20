#ifndef STREAMR_DHT_TYPES_PORTRANGE_HPP
#define STREAMR_DHT_TYPES_PORTRANGE_HPP

#include <cstdint>

namespace streamr::dht::types {

struct PortRange {
    uint16_t min;
    uint16_t max;
};

} // namespace streamr::dht::types

#endif // STREAMR_DHT_TYPES_PORTRANGE_HPP
