// Module streamr.dht.ringIdentifiers
// Ported from packages/dht/src/dht/contact/ringIdentifiers.ts
// (v103.8.0-rc.3).
module;
#include <new>


export module streamr.dht.ringIdentifiers;

import std;

import streamr.dht.protos;

import streamr.utils.Branded;

export namespace streamr::dht::contact {

using ::dht::PeerDescriptor;

// The raw ring id: region (4 bytes, big-endian) + ipAddress (4 bytes,
// big-endian) + the last 7 bytes of the node id = 15 bytes = 120 bits.
// Notice (from TS): you cannot convert a RingId back to RingIdRaw, because
// RingId is only an approximation of the actual value — that is why the raw
// form is what is stored and compared throughout the codebase.
using RingIdRaw = streamr::utils::Branded<std::string, struct RingIdRawBrand>;
// RingId / RingDistance are JavaScript Numbers in TS, i.e. doubles. The raw
// 120-bit value exceeds a double's 53-bit mantissa, so the low bits are
// lost — deliberately, and identically to TS, so orderings match.
using RingId = double;
using RingDistance = double;

namespace ringidentifiers {
inline constexpr std::size_t wordBytes = 4;
inline constexpr std::size_t uniquePartBytes = 7;
inline constexpr int byteBits = 8;
inline constexpr unsigned int byteMask = 0xFF;
} // namespace ringidentifiers

// 2^120 - 1, evaluated as a double exactly as TS's `2 ** 120 - 1` (the -1
// is lost to rounding, giving 2^120).
inline const RingDistance RING_SIZE = // NOLINT(cert-err58-cpp)
    std::ldexp(1.0, 120) - 1.0; // NOLINT(readability-magic-numbers)

// The exact 120-bit big-endian integer, converted to a double once (as
// TS's Number(binaryToBigInt(...)) does) — NOT accumulated byte-by-byte in
// double, which would round differently. 128-bit is wide enough for 120
// bits.
inline double ringIdRawToDouble(const RingIdRaw& raw) {
    unsigned __int128 acc = 0;
    for (const char byte : raw) {
        acc = (acc << ringidentifiers::byteBits) |
            static_cast<unsigned char>(byte);
    }
    return static_cast<double>(acc);
}

inline RingId getRingIdFromRaw(const RingIdRaw& raw) {
    return ringIdRawToDouble(raw);
}

inline RingIdRaw getRingIdRawFromPeerDescriptor(
    const PeerDescriptor& peerDescriptor) {
    std::string raw;
    raw.reserve(
        2 * ringidentifiers::wordBytes + ringidentifiers::uniquePartBytes);
    const auto appendBigEndian = [&raw](std::uint32_t value) {
        for (std::size_t i = ringidentifiers::wordBytes; i-- > 0;) {
            raw.push_back(
                static_cast<char>(
                    (value >> (ringidentifiers::byteBits * i)) &
                    ringidentifiers::byteMask));
        }
    };
    // region()/ipaddress() return 0 when unset (proto3), matching TS's `?? 0`.
    appendBigEndian(peerDescriptor.region());
    appendBigEndian(peerDescriptor.ipaddress());
    const std::string& nodeId = peerDescriptor.nodeid();
    const std::size_t start = nodeId.size() >= ringidentifiers::uniquePartBytes
        ? nodeId.size() - ringidentifiers::uniquePartBytes
        : 0;
    raw.append(nodeId, start, ringidentifiers::uniquePartBytes);
    return RingIdRaw{raw};
}

inline RingId getRingIdFromPeerDescriptor(
    const PeerDescriptor& peerDescriptor) {
    return ringIdRawToDouble(getRingIdRawFromPeerDescriptor(peerDescriptor));
}

inline RingDistance getLeftDistance(RingId referenceId, RingId id) {
    const double diff = std::fabs(referenceId - id);
    // id smaller than referenceId → distance is the difference;
    // otherwise it wraps around the ring.
    return (referenceId > id) ? diff : RING_SIZE - diff;
}

inline RingDistance getRightDistance(RingId referenceId, RingId id) {
    const double diff = std::fabs(referenceId - id);
    return (referenceId > id) ? RING_SIZE - diff : diff;
}

} // namespace streamr::dht::contact
