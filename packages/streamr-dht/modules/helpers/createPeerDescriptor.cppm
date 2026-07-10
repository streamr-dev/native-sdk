// Module streamr.dht.createPeerDescriptor
// Ported from packages/dht/src/helpers/createPeerDescriptor.ts
// (v103.8.0-rc.3). Builds the local node's signed peer descriptor from a
// connectivity response: unless an explicit node id is given, the id is
// derived from the reported ip address as
//   concat(last 13 bytes of keccak(ip), last 7 bytes of sign(ip))
// and the whole descriptor is signed over
// createPeerDescriptorSignaturePayload. The keccak and the signature both
// use the Ethereum message magic (the TS EcdsaSecp256k1Evm defaults), which
// SigningUtils::hash / createSignature implement — wire-visible, keep exact.
// Like TS (its TODO), the key pair is throwaway: a random 32-byte private
// key signs, and the published publicKey is 20 random bytes.
module;

#include <openssl/rand.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

export module streamr.dht.createPeerDescriptor;

import streamr.dht.protos;

import streamr.dht.createPeerDescriptorSignaturePayload;
import streamr.dht.Identifiers;
import streamr.utils.BinaryUtils;
import streamr.utils.SigningUtils;

export namespace streamr::dht::helpers {

using ::dht::ConnectivityResponse;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::utils::BinaryUtils;
using streamr::utils::SigningUtils;

namespace createpeerdescriptordetail {

inline constexpr size_t privateKeyBytes = 32;
inline constexpr size_t publicKeyBytes = 20;
inline constexpr size_t nodeIdHashBytes = 13;
inline constexpr size_t nodeIdSignatureBytes = 7;

inline std::string randomBytes(size_t count) {
    std::string bytes(count, '\0');
    if (RAND_bytes(
            reinterpret_cast<unsigned char*>(bytes.data()), // NOLINT
            static_cast<int>(count)) != 1) {
        throw std::runtime_error("createPeerDescriptor: RAND_bytes failed");
    }
    return bytes;
}

// nodeId is calculated as
// concatenate(
//   get104leastSignificantBits(hash(ipAddress)),
//   get56leastSignificantBits(sign(ipAddress))
// )
inline DhtAddressRaw calculateNodeIdRaw(
    uint32_t ipAddress, const std::string& privateKeyHex) {
    const std::string ipAsBuffer = convertUnsignedIntegerToBuffer(ipAddress);
    const std::string ipHash = SigningUtils::hash(ipAsBuffer);
    const std::string signature =
        SigningUtils::createSignature(ipAsBuffer, privateKeyHex);
    return DhtAddressRaw{
        ipHash.substr(ipHash.size() - nodeIdHashBytes) +
        signature.substr(signature.size() - nodeIdSignatureBytes)};
}

} // namespace createpeerdescriptordetail

inline PeerDescriptor createPeerDescriptor(
    const ConnectivityResponse& connectivityResponse,
    uint32_t region,
    const std::optional<DhtAddress>& nodeId = std::nullopt) {
    namespace detail = createpeerdescriptordetail;
    const std::string privateKeyHex = BinaryUtils::binaryStringToHex(
        detail::randomBytes(detail::privateKeyBytes));
    // TODO calculate publicKey from privateKey (mirrors the TS TODO)
    const std::string publicKey = detail::randomBytes(detail::publicKeyBytes);
    DhtAddressRaw nodeIdRaw = (nodeId.has_value())
        ? Identifiers::getRawFromDhtAddress(*nodeId)
        : detail::calculateNodeIdRaw(
              connectivityResponse.ipaddress(), privateKeyHex);
    PeerDescriptor ret;
    ret.set_nodeid(nodeIdRaw);
    ret.set_type(NodeType::NODEJS);
    ret.set_ipaddress(connectivityResponse.ipaddress());
    ret.set_region(region);
    ret.set_publickey(publicKey);
    if (connectivityResponse.has_websocket()) {
        ret.mutable_websocket()->set_host(
            connectivityResponse.websocket().host());
        ret.mutable_websocket()->set_port(
            connectivityResponse.websocket().port());
        ret.mutable_websocket()->set_tls(
            connectivityResponse.websocket().tls());
    }
    ret.set_signature(
        SigningUtils::createSignature(
            createPeerDescriptorSignaturePayload(ret), privateKeyHex));
    return ret;
}

} // namespace streamr::dht::helpers
