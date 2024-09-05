#ifndef STREAMR_DHT_OFFERER_HPP
#define STREAMR_DHT_OFFERER_HPP

#include <string>
#include <boost/endian/conversion.hpp>
#include <boost/uuid/detail/md5.hpp>
#include "streamr-dht/Identifiers.hpp"

namespace streamr::dht::helpers {

using streamr::dht::DhtAddress;

enum class Offerer { LOCAL, REMOTE };

class OffererHelper {
public:
    static Offerer getOfferer(
        const DhtAddress& localNodeId, const DhtAddress& remoteNodeId) { // NOLINT
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
            *reinterpret_cast<int32_t*>(digest));
    }
};

} // namespace streamr::dht::helpers

#endif // STREAMR_DHT_OFFERER_HPP