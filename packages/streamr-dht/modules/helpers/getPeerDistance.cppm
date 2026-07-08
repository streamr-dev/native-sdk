// Module streamr.dht.getPeerDistance
// Ported from packages/dht/src/dht/helpers/getPeerDistance.ts
// (v103.8.0-rc.3).
module;


export module streamr.dht.getPeerDistance;

import std;

import streamr.dht.Identifiers;

export namespace streamr::dht::helpers {

using streamr::dht::DhtAddressRaw;

// XOR distance between two raw ids, computed exactly as the npm `k-bucket`
// library's KBucket.distance (which the TS getPeerDistance wraps): the
// bytes are folded into a single number, most-significant byte first. The
// accumulator is a double, matching JavaScript's Number, so for the full
// 20-byte kademlia ids the low bits are lost the same way they are in TS
// — the value is only used as a sort key, and both this port and the TS
// tests fold with the identical IEEE-754 operations, so the ordering
// matches. (KBucket itself arrives in phase A2; only this distance
// function is needed here.)
inline double getPeerDistance(
    const DhtAddressRaw& first, const DhtAddressRaw& second) {
    double distance = 0;
    const std::size_t min = std::min(first.size(), second.size());
    const std::size_t max = std::max(first.size(), second.size());
    std::size_t i = 0;
    for (; i < min; ++i) {
        const auto a = static_cast<unsigned char>(first[i]);
        const auto b = static_cast<unsigned char>(second[i]);
        distance =
            distance * 256 + (a ^ b); // NOLINT(readability-magic-numbers)
    }
    for (; i < max; ++i) {
        distance = distance * 256 + 255; // NOLINT(readability-magic-numbers)
    }
    return distance;
}

} // namespace streamr::dht::helpers
