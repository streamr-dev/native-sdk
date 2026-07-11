// Module streamr.trackerlessnetwork.streamPartIdToDataKey
// Ported from streamPartIdToDataKey() in packages/trackerless-network/
// src/ContentDeliveryManager.ts (v103.8.0-rc.3): stream-part entry
// points are stored in the DHT under SHA1(streamPartId). The key must
// match the TS derivation byte for byte, or C++ and TS nodes would
// store/fetch the same stream part's entry points under different DHT
// keys (wire compatibility — TypeScript wins).
module;

#include <string>
#include <openssl/evp.h>

export module streamr.trackerlessnetwork.streamPartIdToDataKey;

import streamr.dht.Identifiers;
import streamr.utils.StreamPartID;

using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork {

inline DhtAddress streamPartIdToDataKey(const StreamPartID& streamPartId) {
    unsigned char digest[EVP_MAX_MD_SIZE]; // NOLINT
    unsigned int digestLength = 0;
    EVP_Digest(
        streamPartId.data(),
        streamPartId.size(),
        digest, // NOLINT
        &digestLength,
        EVP_sha1(),
        nullptr);
    return Identifiers::getDhtAddressFromRaw(
        DhtAddressRaw{std::string(
            reinterpret_cast<const char*>(digest), digestLength)}); // NOLINT
}

} // namespace streamr::trackerlessnetwork
