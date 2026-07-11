// Module streamr.trackerlessnetwork.DiscoveryLayerNode
// Ported from packages/trackerless-network/src/discovery-layer/
// DiscoveryLayerNode.ts (v103.8.0-rc.3): the control layer's view of a
// stream part's layer-1 DHT node. The production implementation wraps a
// streamr::dht::DhtNode (phase C6's createDiscoveryLayerNode); unit tests
// substitute streamr.trackerlessnetwork.MockDiscoveryLayerNode.
module;

#include <cstddef>
#include <optional>
#include <tuple>
#include <vector>

#include <coroutine> // IWYU pragma: keep

export module streamr.trackerlessnetwork.DiscoveryLayerNode;

import streamr.dht.protos;

// export import: the interface returns ClosestRingPeerDescriptors, so
// every consumer needs the declaration visible.
export import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.Identifiers;
import streamr.eventemitter.EventEmitter;
import streamr.utils.CoroutineHelper;

export namespace streamr::trackerlessnetwork::discoverylayer {

using ::dht::PeerDescriptor;
// TS RingContacts ({ left, right } of PeerDescriptors) — the dht package's
// ClosestRingPeerDescriptors is that exact shape and what
// DhtNode::getRingContacts() returns.
using streamr::dht::ClosestRingPeerDescriptors;
using streamr::dht::DhtAddress;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

namespace discoverylayernodeevents {

struct ManualRejoinRequired : Event<> {};
struct NearbyContactAdded : Event<PeerDescriptor> {};
struct NearbyContactRemoved : Event<PeerDescriptor> {};
struct RandomContactAdded : Event<PeerDescriptor> {};
struct RandomContactRemoved : Event<PeerDescriptor> {};
struct RingContactAdded : Event<PeerDescriptor> {};
struct RingContactRemoved : Event<PeerDescriptor> {};

} // namespace discoverylayernodeevents

using DiscoveryLayerNodeEvents = std::tuple<
    discoverylayernodeevents::ManualRejoinRequired,
    discoverylayernodeevents::NearbyContactAdded,
    discoverylayernodeevents::NearbyContactRemoved,
    discoverylayernodeevents::RandomContactAdded,
    discoverylayernodeevents::RandomContactRemoved,
    discoverylayernodeevents::RingContactAdded,
    discoverylayernodeevents::RingContactRemoved>;

class DiscoveryLayerNode : public EventEmitter<DiscoveryLayerNodeEvents> {
public:
    virtual void removeContact(const DhtAddress& nodeId) = 0;
    [[nodiscard]] virtual std::vector<PeerDescriptor> getClosestContacts(
        std::optional<size_t> maxCount = std::nullopt) = 0;
    [[nodiscard]] virtual std::vector<PeerDescriptor> getRandomContacts(
        std::optional<size_t> maxCount = std::nullopt) = 0;
    [[nodiscard]] virtual ClosestRingPeerDescriptors getRingContacts() = 0;
    [[nodiscard]] virtual std::vector<PeerDescriptor> getNeighbors() = 0;
    [[nodiscard]] virtual size_t getNeighborCount() = 0;
    virtual folly::coro::Task<void> joinDht(
        std::vector<PeerDescriptor> entryPoints,
        bool doRandomJoin = true,
        bool retry = true) = 0;
    virtual folly::coro::Task<void> joinRing() = 0;
    virtual folly::coro::Task<void> start() = 0;
    virtual folly::coro::Task<void> stop() = 0;
};

} // namespace streamr::trackerlessnetwork::discoverylayer
