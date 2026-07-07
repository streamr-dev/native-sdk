// Module streamr.dht.getPreviousPeer
// Ported from packages/dht/src/dht/routing/getPreviousPeer.ts
// (v103.8.0-rc.3). The previous hop is the last entry appended to the
// routed message's routingPath.
module;

#include <optional>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.getPreviousPeer;

export namespace streamr::dht::routing {

using ::dht::PeerDescriptor;
using ::dht::RouteMessageWrapper;

inline std::optional<PeerDescriptor> getPreviousPeer(
    const RouteMessageWrapper& routeMessage) {
    if (routeMessage.routingpath_size() == 0) {
        return std::nullopt;
    }
    return routeMessage.routingpath(routeMessage.routingpath_size() - 1);
}

} // namespace streamr::dht::routing
