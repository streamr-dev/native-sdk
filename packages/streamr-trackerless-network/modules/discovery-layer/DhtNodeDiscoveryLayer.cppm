// Module streamr.trackerlessnetwork.DhtNodeDiscoveryLayer
// The TS code passes a DhtNode wherever a DiscoveryLayerNode is needed
// (structural typing); this adapter provides that shape nominally: it
// implements the DiscoveryLayerNode interface over a layer-1
// streamr::dht::DhtNode, forwarding the six contact events and
// delegating the getters and lifecycle.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module streamr.trackerlessnetwork.DhtNodeDiscoveryLayer;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.PeerManager;
import streamr.dht.protos;

using streamr::dht::ClosestRingPeerDescriptors;
using streamr::dht::DhtAddress;
using streamr::dht::DhtNode;

export namespace streamr::trackerlessnetwork::discoverylayer {

using ::dht::PeerDescriptor;

class DhtNodeDiscoveryLayer : public DiscoveryLayerNode {
private:
    std::shared_ptr<DhtNode> dhtNode;

    template <typename SourceEvent, typename TargetEvent>
    void forward() {
        this->dhtNode->template on<SourceEvent>(
            [this](const PeerDescriptor& peerDescriptor) {
                this->template emit<TargetEvent>(peerDescriptor);
            });
    }

public:
    explicit DhtNodeDiscoveryLayer(std::shared_ptr<DhtNode> dhtNode)
        : dhtNode(std::move(dhtNode)) {
        namespace pme = streamr::dht::peermanagerevents;
        namespace dle = discoverylayernodeevents;
        this->forward<pme::NearbyContactAdded, dle::NearbyContactAdded>();
        this->forward<pme::NearbyContactRemoved, dle::NearbyContactRemoved>();
        this->forward<pme::RandomContactAdded, dle::RandomContactAdded>();
        this->forward<pme::RandomContactRemoved, dle::RandomContactRemoved>();
        this->forward<pme::RingContactAdded, dle::RingContactAdded>();
        this->forward<pme::RingContactRemoved, dle::RingContactRemoved>();
    }

    [[nodiscard]] DhtNode& getDhtNode() { return *this->dhtNode; }

    void removeContact(const DhtAddress& nodeId) override {
        this->dhtNode->removeContact(nodeId);
    }

    [[nodiscard]] std::vector<PeerDescriptor> getClosestContacts(
        std::optional<size_t> maxCount = std::nullopt) override {
        return this->dhtNode->getClosestContacts(maxCount);
    }

    [[nodiscard]] std::vector<PeerDescriptor> getRandomContacts(
        std::optional<size_t> maxCount = std::nullopt) override {
        return this->dhtNode->getRandomContacts(maxCount);
    }

    [[nodiscard]] ClosestRingPeerDescriptors getRingContacts() override {
        return this->dhtNode->getRingContacts();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getNeighbors() override {
        return this->dhtNode->getNeighbors();
    }

    [[nodiscard]] size_t getNeighborCount() override {
        return this->dhtNode->getNeighborCount();
    }

    folly::coro::Task<void> joinDht(
        std::vector<PeerDescriptor> entryPoints,
        bool doRandomJoin = true,
        bool retry = true) override {
        co_await this->dhtNode->joinDht(
            std::move(entryPoints), doRandomJoin, retry);
    }

    folly::coro::Task<void> joinRing() override {
        co_await this->dhtNode->joinRing();
    }

    folly::coro::Task<void> start() override {
        co_await this->dhtNode->start();
    }

    folly::coro::Task<void> stop() override {
        this->dhtNode->stop();
        co_return;
    }
};

} // namespace streamr::trackerlessnetwork::discoverylayer
