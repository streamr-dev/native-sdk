// Module streamr.trackerlessnetwork.createContentDeliveryLayerNode
// Ported from packages/trackerless-network/src/content-delivery-layer/
// createContentDeliveryLayerNode.ts (v103.8.0-rc.3): fills the optional
// components of ContentDeliveryLayerNode with defaults built from the
// exact TS default values (neighbor target 4, view size 20, min
// propagation targets 2, buffer 150 / 10 s, update interval 10 s).
// The plumtree branch is milestone-E scope and omitted; the TS
// bufferWhileConnecting send flag is not plumbed through the C++
// send options yet (documented deviation — propagation retries cover
// the connecting window).
module;

#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module streamr.trackerlessnetwork.createContentDeliveryLayerNode;

import streamr.trackerlessnetwork.protos;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryLayerNode;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.trackerlessnetwork.formStreamPartDeliveryServiceId;
import streamr.trackerlessnetwork.Handshaker;
import streamr.trackerlessnetwork.Inspector;
import streamr.trackerlessnetwork.NeighborFinder;
import streamr.trackerlessnetwork.NeighborUpdateManager;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.Propagation;
import streamr.trackerlessnetwork.ProxyConnectionRpcLocal;
import streamr.trackerlessnetwork.TemporaryConnectionRpcLocal;
import streamr.dht.ConnectionLocker;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.Transport;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using streamr::trackerlessnetwork::discoverylayer::DiscoveryLayerNode;
using streamr::trackerlessnetwork::inspection::Inspector;
using streamr::trackerlessnetwork::inspection::InspectorOptions;
using streamr::trackerlessnetwork::neighbordiscovery::
    defaultNeighborUpdateInterval;
using streamr::trackerlessnetwork::neighbordiscovery::Handshaker;
using streamr::trackerlessnetwork::neighbordiscovery::HandshakerOptions;
using streamr::trackerlessnetwork::neighbordiscovery::INeighborFinder;
using streamr::trackerlessnetwork::neighbordiscovery::NeighborFinder;
using streamr::trackerlessnetwork::neighbordiscovery::NeighborFinderOptions;
using streamr::trackerlessnetwork::neighbordiscovery::NeighborUpdateManager;
using streamr::trackerlessnetwork::neighbordiscovery::
    NeighborUpdateManagerOptions;
using streamr::trackerlessnetwork::propagation::DEFAULT_MAX_MESSAGES;
using streamr::trackerlessnetwork::propagation::DEFAULT_TTL;
using streamr::trackerlessnetwork::propagation::Propagation;
using streamr::trackerlessnetwork::propagation::PropagationOptions;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcLocal;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcLocalOptions;

constexpr size_t defaultMinPropagationTargets = 2;

struct ContentDeliveryLayerNodeOptions {
    StreamPartID streamPartId;
    std::shared_ptr<DiscoveryLayerNode> discoveryLayerNode;
    Transport* transport = nullptr;
    ConnectionLocker* connectionLocker = nullptr;
    PeerDescriptor localPeerDescriptor;
    std::function<bool()> isLocalNodeEntryPoint;
    // Optional component overrides (tests inject doubles here).
    std::shared_ptr<ListeningRpcCommunicator> rpcCommunicator;
    std::shared_ptr<NodeList> neighbors;
    std::shared_ptr<NodeList> leftNodeView;
    std::shared_ptr<NodeList> rightNodeView;
    std::shared_ptr<NodeList> nearbyNodeView;
    std::shared_ptr<NodeList> randomNodeView;
    std::shared_ptr<Handshaker> handshaker;
    std::shared_ptr<INeighborFinder> neighborFinder;
    std::shared_ptr<NeighborUpdateManager> neighborUpdateManager;
    std::shared_ptr<Propagation> propagation;
    std::shared_ptr<Inspector> inspector;
    // Optional tuning values (TS defaults applied when absent).
    std::optional<size_t> neighborTargetCount;
    std::optional<size_t> maxContactCount;
    std::optional<size_t> minPropagationTargets;
    std::optional<size_t> maxPropagationBufferSize;
    std::optional<bool> acceptProxyConnections;
    std::optional<std::chrono::milliseconds> neighborUpdateInterval;
    std::optional<std::chrono::milliseconds> rpcRequestTimeout;
    bool suppressOwnMessageLoopback = false;
};

inline std::shared_ptr<ContentDeliveryLayerNode> createContentDeliveryLayerNode(
    ContentDeliveryLayerNodeOptions options) {
    const auto ownNodeId =
        Identifiers::getNodeIdFromPeerDescriptor(options.localPeerDescriptor);
    const auto neighborTargetCount =
        options.neighborTargetCount.value_or(defaultNeighborTargetCount);
    const auto maxContactCount =
        options.maxContactCount.value_or(defaultNodeViewSize);
    const auto acceptProxyConnections =
        options.acceptProxyConnections.value_or(defaultAcceptProxyConnections);

    auto rpcCommunicator = options.rpcCommunicator
        ? options.rpcCommunicator
        : std::make_shared<ListeningRpcCommunicator>(
              formStreamPartContentDeliveryServiceId(options.streamPartId),
              *options.transport);
    const auto makeList = [&ownNodeId, maxContactCount]() {
        return std::make_shared<NodeList>(ownNodeId, maxContactCount);
    };
    auto neighbors = options.neighbors ? options.neighbors : makeList();
    auto leftNodeView =
        options.leftNodeView ? options.leftNodeView : makeList();
    auto rightNodeView =
        options.rightNodeView ? options.rightNodeView : makeList();
    auto nearbyNodeView =
        options.nearbyNodeView ? options.nearbyNodeView : makeList();
    auto randomNodeView =
        options.randomNodeView ? options.randomNodeView : makeList();
    auto ongoingHandshakes = std::make_shared<std::set<DhtAddress>>();

    auto temporaryConnectionRpcLocal =
        std::make_shared<TemporaryConnectionRpcLocal>(
            TemporaryConnectionRpcLocalOptions{
                .localPeerDescriptor = options.localPeerDescriptor,
                .streamPartId = options.streamPartId,
                .rpcCommunicator = *rpcCommunicator,
                .connectionLocker = *options.connectionLocker});
    std::shared_ptr<ProxyConnectionRpcLocal> proxyConnectionRpcLocal;
    if (acceptProxyConnections) {
        proxyConnectionRpcLocal = std::make_shared<ProxyConnectionRpcLocal>(
            ProxyConnectionRpcLocalOptions{
                .localPeerDescriptor = options.localPeerDescriptor,
                .streamPartId = options.streamPartId,
                .rpcCommunicator = *rpcCommunicator});
    }
    auto propagation = options.propagation
        ? options.propagation
        : std::make_shared<Propagation>(PropagationOptions{
              .sendToNeighbor =
                  [neighbors,
                   temporaryConnectionRpcLocal,
                   proxyConnectionRpcLocal](
                      const DhtAddress& neighborId,
                      const StreamMessage& msg) -> folly::coro::Task<void> {
                  auto remote = neighbors->get(neighborId);
                  if (!remote.has_value()) {
                      remote = temporaryConnectionRpcLocal->getNodes().get(
                          neighborId);
                  }
                  if (remote.has_value()) {
                      co_await remote.value()->sendStreamMessage(msg);
                      co_return;
                  }
                  if (proxyConnectionRpcLocal) {
                      const auto proxyConnection =
                          proxyConnectionRpcLocal->getConnection(neighborId);
                      if (proxyConnection.has_value()) {
                          co_await proxyConnection.value()
                              ->remote.sendStreamMessage(msg);
                          co_return;
                      }
                  }
                  throw std::runtime_error("Propagation target not found");
              },
              .minPropagationTargets = options.minPropagationTargets.value_or(
                  defaultMinPropagationTargets),
              .ttl = DEFAULT_TTL,
              .maxMessages = options.maxPropagationBufferSize.value_or(
                  DEFAULT_MAX_MESSAGES)});
    auto handshaker = options.handshaker
        ? options.handshaker
        : std::make_shared<Handshaker>(HandshakerOptions{
              .localPeerDescriptor = options.localPeerDescriptor,
              .streamPartId = options.streamPartId,
              .neighbors = *neighbors,
              .leftNodeView = *leftNodeView,
              .rightNodeView = *rightNodeView,
              .nearbyNodeView = *nearbyNodeView,
              .randomNodeView = *randomNodeView,
              .rpcCommunicator = *rpcCommunicator,
              .maxNeighborCount = neighborTargetCount,
              .ongoingHandshakes = *ongoingHandshakes,
              .rpcRequestTimeout = options.rpcRequestTimeout});
    auto neighborFinder = options.neighborFinder
        ? options.neighborFinder
        : std::static_pointer_cast<INeighborFinder>(
              std::make_shared<NeighborFinder>(NeighborFinderOptions{
                  .neighbors = *neighbors,
                  .nearbyNodeView = *nearbyNodeView,
                  .leftNodeView = *leftNodeView,
                  .rightNodeView = *rightNodeView,
                  .randomNodeView = *randomNodeView,
                  .doFindNeighbors =
                      [handshaker](std::vector<DhtAddress> excludedIds) {
                          return handshaker->attemptHandshakesOnContacts(
                              std::move(excludedIds));
                      },
                  .minCount = neighborTargetCount}));
    auto neighborUpdateManager = options.neighborUpdateManager
        ? options.neighborUpdateManager
        : std::make_shared<NeighborUpdateManager>(NeighborUpdateManagerOptions{
              .localPeerDescriptor = options.localPeerDescriptor,
              .streamPartId = options.streamPartId,
              .neighbors = *neighbors,
              .nearbyNodeView = *nearbyNodeView,
              .neighborFinder = *neighborFinder,
              .rpcCommunicator = *rpcCommunicator,
              .neighborUpdateInterval = options.neighborUpdateInterval.value_or(
                  defaultNeighborUpdateInterval),
              .neighborTargetCount = neighborTargetCount,
              .ongoingHandshakes = *ongoingHandshakes});
    auto inspector = options.inspector
        ? options.inspector
        : std::make_shared<Inspector>(InspectorOptions{
              .localPeerDescriptor = options.localPeerDescriptor,
              .streamPartId = options.streamPartId,
              .rpcCommunicator = *rpcCommunicator,
              .connectionLocker = *options.connectionLocker});

    return std::make_shared<ContentDeliveryLayerNode>(
        StrictContentDeliveryLayerNodeOptions{
            .streamPartId = options.streamPartId,
            .discoveryLayerNode = options.discoveryLayerNode,
            .transport = options.transport,
            .connectionLocker = options.connectionLocker,
            .localPeerDescriptor = options.localPeerDescriptor,
            .nodeViewSize = maxContactCount,
            .rpcCommunicator = rpcCommunicator,
            .ongoingHandshakes = ongoingHandshakes,
            .nearbyNodeView = nearbyNodeView,
            .randomNodeView = randomNodeView,
            .leftNodeView = leftNodeView,
            .rightNodeView = rightNodeView,
            .neighbors = neighbors,
            .temporaryConnectionRpcLocal = temporaryConnectionRpcLocal,
            .proxyConnectionRpcLocal = proxyConnectionRpcLocal,
            .propagation = propagation,
            .handshaker = handshaker,
            .neighborFinder = neighborFinder,
            .neighborUpdateManager = neighborUpdateManager,
            .inspector = inspector,
            .neighborTargetCount = neighborTargetCount,
            .isLocalNodeEntryPoint = std::move(options.isLocalNodeEntryPoint),
            .rpcRequestTimeout = options.rpcRequestTimeout,
            .suppressOwnMessageLoopback = options.suppressOwnMessageLoopback});
}

} // namespace streamr::trackerlessnetwork
