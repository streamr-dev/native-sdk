// Module streamr.trackerlessnetwork.formStreamPartDeliveryServiceId
// CONSOLIDATED from the former header logic/formStreamPartDeliveryServiceId.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

// The string concatenation below needs std::operator+ to be TEXTUALLY
// visible: operators reached only through an imported module's global
// module fragment are not reliably reachable. (The former header got
// <string> transitively from the sibling headers it included.)
#include <string>

export module streamr.trackerlessnetwork.formStreamPartDeliveryServiceId;

import streamr.dht.Identifiers;
import streamr.utils;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::ServiceID;
using streamr::utils::StreamPartID;
export namespace streamr::trackerlessnetwork {

inline ServiceID formStreamPartContentDeliveryServiceId(
    const StreamPartID& streamPartId) {
    // could be "content-delivery" instead of "delivery", but that is a breaking
    // change
    return ServiceID{"stream-part-delivery-" + streamPartId};
}

} // namespace streamr::trackerlessnetwork
