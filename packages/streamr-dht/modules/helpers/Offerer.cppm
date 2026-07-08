// Module streamr.dht.Offerer
// CONSOLIDATED from the former header streamr-dht/helpers/Offerer.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <boost/endian/conversion.hpp>
#include <boost/uuid/detail/md5.hpp>

export module streamr.dht.Offerer;

import std;

import streamr.dht.Identifiers;
export namespace streamr::dht::helpers {

using streamr::dht::DhtAddress;

enum class Offerer : std::uint8_t { LOCAL, REMOTE };

class OffererHelper {
public:
    static Offerer getOfferer(
        const DhtAddress& localNodeId,
        const DhtAddress& remoteNodeId) { // NOLINT
        return getOfferingHash(localNodeId + "," + remoteNodeId) <
                getOfferingHash(remoteNodeId + "," + localNodeId)
            ? Offerer::LOCAL
            : Offerer::REMOTE;
    }

    static int getOfferingHash(const std::string& idPair) {
        boost::uuids::detail::md5 hash;
        hash.process_bytes(idPair.data(), idPair.size());
        boost::uuids::detail::md5::digest_type digest;
        hash.get_digest(digest);
        return boost::endian::little_to_native(
            *reinterpret_cast<std::int32_t*>(digest));
    }
};

} // namespace streamr::dht::helpers
