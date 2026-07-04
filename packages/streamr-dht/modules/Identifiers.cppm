// Module streamr.dht.Identifiers
// CONSOLIDATED from the former header streamr-dht/Identifiers.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <cstdint>
#include <string>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.Identifiers;

import streamr.utils;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::utils::BinaryUtils;
export namespace streamr::dht {

using DhtAddress = streamr::utils::Branded<std::string, struct DhtAddressBrand>;
using DhtAddressRaw =
    streamr::utils::Branded<std::string, struct DhtAddressRawBrand>;

using ServiceID = streamr::utils::Branded<std::string, struct ServiceIDBrand>;

inline constexpr size_t kademliaIdLengthInBytes = 20;

struct Identifiers {
    using PeerDescriptor = ::dht::PeerDescriptor;

    static DhtAddress getDhtAddressFromRaw(const DhtAddressRaw& raw) {
        return DhtAddress{BinaryUtils::binaryStringToHex(raw)};
    }

    static DhtAddressRaw getRawFromDhtAddress(const DhtAddress& address) {
        return DhtAddressRaw{BinaryUtils::hexToBinaryString(address)};
    }

    static DhtAddress getNodeIdFromPeerDescriptor(
        const PeerDescriptor& peerDescriptor) {
        return getDhtAddressFromRaw(DhtAddressRaw{peerDescriptor.nodeid()});
    }

    static bool areEqualPeerDescriptors(
        const PeerDescriptor& peerDescriptor1,
        const PeerDescriptor& peerDescriptor2) {
        return (peerDescriptor1.nodeid() == peerDescriptor2.nodeid());
    }

    static DhtAddress createRandomDhtAddress() {
        return getDhtAddressFromRaw(DhtAddressRaw{[&]() {
            std::vector<uint8_t> randomBytes(kademliaIdLengthInBytes);
            std::ranges::generate(randomBytes, []() {
                return static_cast<uint8_t>(std::rand() % 256); // NOLINT
            });
            return std::string(randomBytes.begin(), randomBytes.end());
        }()});
    }
};
} // namespace streamr::dht
