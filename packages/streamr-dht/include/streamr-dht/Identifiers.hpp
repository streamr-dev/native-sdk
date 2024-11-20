#ifndef STREAMR_DHT_IDENTIFIERS_HPP
#define STREAMR_DHT_IDENTIFIERS_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-utils/BinaryUtils.hpp"
#include "streamr-utils/Branded.hpp"
namespace streamr::dht {

using streamr::utils::BinaryUtils;

using DhtAddress = streamr::utils::Branded<std::string, struct DhtAddressBrand>;
using DhtAddressRaw =
    streamr::utils::Branded<std::string, struct DhtAddressRawBrand>;

using ServiceID = streamr::utils::Branded<std::string, struct ServiceIDBrand>;

constexpr size_t kademliaIdLengthInBytes = 20;

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
            std::generate(randomBytes.begin(), randomBytes.end(), []() {
                return static_cast<uint8_t>(std::rand() % 256); // NOLINT
            });
            return std::string(randomBytes.begin(), randomBytes.end());
        }()});
    }
};
} // namespace streamr::dht

#endif // STREAMR_DHT_IDENTIFIERS_HPP