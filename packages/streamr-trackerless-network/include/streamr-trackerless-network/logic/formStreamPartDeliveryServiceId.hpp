#ifndef STREAMR_TRACKERLESS_NETWORK_FORM_STREAM_PART_DELIVERY_SERVICE_ID_HPP
#define STREAMR_TRACKERLESS_NETWORK_FORM_STREAM_PART_DELIVERY_SERVICE_ID_HPP

#include "streamr-dht/Identifiers.hpp"
#include "streamr-utils/StreamPartID.hpp"

namespace streamr::trackerlessnetwork {

using dht::ServiceID;
using utils::StreamPartID;

inline ServiceID formStreamPartContentDeliveryServiceId(
    const StreamPartID& streamPartId) {
    // could be "content-delivery" instead of "delivery", but that is a breaking
    // change
    return ServiceID{"stream-part-delivery-" + streamPartId};
}

} // namespace streamr::trackerlessnetwork

#endif // STREAMR_TRACKERLESS_NETWORK_FORM_STREAM_PART_DELIVERY_SERVICE_ID_HPP