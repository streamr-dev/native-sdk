// Module streamr.trackerlessnetwork.createStreamPartDiscoveryLayerNode
// The layer-1 discovery node construction from TS
// ContentDeliveryManager.createDiscoveryLayerNode (v103.8.0-rc.3),
// extracted into its own module: composing the DhtNode module graph
// inside ContentDeliveryManager.cppm exhausts clang's per-TU
// source-location space, so the manager takes this as an injected
// factory instead (NetworkStack and the tests supply it).
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <memory>
#include <string>
#include <vector>

export module streamr.trackerlessnetwork.createStreamPartDiscoveryLayerNode;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ControlLayerNode;
import streamr.trackerlessnetwork.DhtNodeDiscoveryLayer;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::ServiceID;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork::controllayer {

using ::dht::PeerDescriptor;
using streamr::trackerlessnetwork::discoverylayer::DhtNodeDiscoveryLayer;
using streamr::trackerlessnetwork::discoverylayer::DiscoveryLayerNode;

inline std::shared_ptr<DiscoveryLayerNode> createStreamPartDiscoveryLayerNode(
    const StreamPartID& streamPartId,
    std::vector<PeerDescriptor> entryPoints,
    ControlLayerNode& controlLayerNode) {
    auto dhtNode = std::make_shared<DhtNode>(DhtNodeOptions{
        .serviceId = ServiceID{"layer1::" + streamPartId},
        // TS TODO preserved: use options options or named constants?
        .numberOfNodesPerKBucket = 4,
        .dhtJoinTimeout = std::chrono::milliseconds(20000),
        .neighborPingLimit = 16,
        // TS EXISTING_CONNECTION_TIMEOUT (RpcRemote).
        .rpcRequestTimeout = std::chrono::milliseconds(5000),
        .transport = controlLayerNode.asTransport(),
        .connectionsView = controlLayerNode.getConnectionsView(),
        .peerDescriptor = controlLayerNode.getLocalPeerDescriptor(),
        .entryPoints = std::move(entryPoints)});
    return std::make_shared<DhtNodeDiscoveryLayer>(std::move(dhtNode));
}

} // namespace streamr::trackerlessnetwork::controllayer
