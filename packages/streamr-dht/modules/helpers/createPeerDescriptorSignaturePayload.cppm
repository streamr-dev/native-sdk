// Module streamr.dht.createPeerDescriptorSignaturePayload
// Ported from packages/dht/src/helpers/createPeerDescriptorSignaturePayload.ts
// (v103.8.0-rc.3). Builds the byte payload a peer descriptor's signature is
// computed over: the ','-separated concatenation of the descriptor's type,
// udp/tcp/websocket connectivity methods (protobuf-serialized), region,
// ipAddress and publicKey. Wire-visible — must stay bit-exact with TS
// (protobuf-ts toBinary == protobuf SerializeAsString for the same message).
module;

#include <cstdint>
#include <string>

export module streamr.dht.createPeerDescriptorSignaturePayload;

import streamr.dht.protos;

export namespace streamr::dht::helpers {

using ::dht::PeerDescriptor;

// Buffer.alloc(4).writeUInt32BE(number)
inline std::string convertUnsignedIntegerToBuffer(uint32_t number) {
    std::string buffer(4, '\0');
    buffer[0] = static_cast<char>((number >> 24U) & 0xFFU); // NOLINT
    buffer[1] = static_cast<char>((number >> 16U) & 0xFFU); // NOLINT
    buffer[2] = static_cast<char>((number >> 8U) & 0xFFU); // NOLINT
    buffer[3] = static_cast<char>(number & 0xFFU); // NOLINT
    return buffer;
}

inline std::string createPeerDescriptorSignaturePayload(
    const PeerDescriptor& peerDescriptor) {
    // The TS scalar fields (type, region, ipAddress, publicKey) are always
    // defined on a generated protobuf-ts object, so they are always included;
    // the message fields (udp, tcp, websocket) only when present.
    const std::string separator = ",";
    std::string payload;
    payload += convertUnsignedIntegerToBuffer(
        static_cast<uint32_t>(peerDescriptor.type()));
    payload += separator;
    if (peerDescriptor.has_udp()) {
        payload += peerDescriptor.udp().SerializeAsString();
    }
    payload += separator;
    if (peerDescriptor.has_tcp()) {
        payload += peerDescriptor.tcp().SerializeAsString();
    }
    payload += separator;
    if (peerDescriptor.has_websocket()) {
        payload += peerDescriptor.websocket().SerializeAsString();
    }
    payload += separator;
    payload += convertUnsignedIntegerToBuffer(peerDescriptor.region());
    payload += separator;
    payload += convertUnsignedIntegerToBuffer(peerDescriptor.ipaddress());
    payload += separator;
    payload += peerDescriptor.publickey();
    return payload;
}

} // namespace streamr::dht::helpers
