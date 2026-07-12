// Module streamr.trackerlessnetwork.ControlLayerNode
// Ported from packages/trackerless-network/src/control-layer/
// ControlLayerNode.ts (v103.8.0-rc.3): the layer-0 facade the
// ContentDeliveryManager and NetworkStack talk to. TS extends
// ITransport structurally; here asTransport() exposes the transport
// identity nominally (the layer-1 discovery nodes use the layer-0 node
// itself as their signalling transport).
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <vector>

export module streamr.trackerlessnetwork.ControlLayerNode;

import streamr.utils.CoroutineHelper;
import streamr.dht.ConnectionsView;
import streamr.dht.Identifiers;
import streamr.dht.Transport;
import streamr.dht.protos;

using streamr::dht::DhtAddress;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::transport::Transport;

export namespace streamr::trackerlessnetwork::controllayer {

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using ::google::protobuf::Any;

class ControlLayerNode {
public:
    virtual ~ControlLayerNode() = default;

    virtual folly::coro::Task<void> joinDht(
        std::vector<PeerDescriptor> entryPointDescriptors) = 0;
    [[nodiscard]] virtual bool hasJoined() = 0;
    [[nodiscard]] virtual PeerDescriptor getLocalPeerDescriptor() = 0;
    virtual folly::coro::Task<std::vector<DataEntry>> fetchDataFromDht(
        DhtAddress key) = 0;
    virtual folly::coro::Task<std::vector<PeerDescriptor>> storeDataToDht(
        DhtAddress key, Any data) = 0;
    virtual folly::coro::Task<void> deleteDataFromDht(
        DhtAddress key, bool waitForCompletion) = 0;
    virtual folly::coro::Task<void> waitForNetworkConnectivity() = 0;
    // The layer-0 node as a Transport (TS passes the node itself where a
    // transport is expected).
    [[nodiscard]] virtual Transport* asTransport() = 0;
    [[nodiscard]] virtual std::vector<PeerDescriptor> getNeighbors() = 0;
    [[nodiscard]] virtual ConnectionsView* getConnectionsView() = 0;
    virtual folly::coro::Task<void> start() = 0;
    virtual folly::coro::Task<void> stop() = 0;
};

} // namespace streamr::trackerlessnetwork::controllayer
