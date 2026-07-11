// Module streamr.trackerlessnetwork.MockDiscoveryLayerNode
// Ported from packages/trackerless-network/test/utils/mock/
// MockDiscoveryLayerNode.ts (v103.8.0-rc.3): an in-memory
// DiscoveryLayerNode whose k-bucket grows only via
// addNewRandomPeerToKBucket(); join/start/stop are no-ops.
module;

#include <cstddef>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <coroutine> // IWYU pragma: keep

export module streamr.trackerlessnetwork.MockDiscoveryLayerNode;

import streamr.dht.protos;

import streamr.dht.Identifiers;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.trackerlessnetwork.TestUtils;
import streamr.utils.CoroutineHelper;

export namespace streamr::trackerlessnetwork::testutils {

using ::dht::PeerDescriptor;
using streamr::dht::ClosestRingPeerDescriptors;
using streamr::dht::DhtAddress;
using streamr::trackerlessnetwork::discoverylayer::DiscoveryLayerNode;

class MockDiscoveryLayerNode : public DiscoveryLayerNode {
private:
    // TS runs single-threaded; here the test thread grows the k-bucket
    // while a detached loop (e.g. StreamPartReconnect's) reads it on a
    // pool thread.
    std::mutex mutex;
    std::vector<PeerDescriptor> kbucketPeers;
    std::vector<PeerDescriptor> closestContacts;
    std::vector<PeerDescriptor> randomContacts;

public:
    void removeContact(const DhtAddress& /*nodeId*/) override {}

    [[nodiscard]] std::vector<PeerDescriptor> getClosestContacts(
        std::optional<size_t> /*maxCount*/) override {
        return this->closestContacts;
    }

    void setClosestContacts(std::vector<PeerDescriptor> contacts) {
        this->closestContacts = std::move(contacts);
    }

    [[nodiscard]] std::vector<PeerDescriptor> getRandomContacts(
        std::optional<size_t> /*maxCount*/) override {
        return this->randomContacts;
    }

    void setRandomContacts(std::vector<PeerDescriptor> contacts) {
        this->randomContacts = std::move(contacts);
    }

    [[nodiscard]] ClosestRingPeerDescriptors getRingContacts() override {
        return ClosestRingPeerDescriptors{.left = {}, .right = {}};
    }

    [[nodiscard]] std::vector<PeerDescriptor> getNeighbors() override {
        std::scoped_lock lock(this->mutex);
        return this->kbucketPeers;
    }

    [[nodiscard]] size_t getNeighborCount() override {
        std::scoped_lock lock(this->mutex);
        return this->kbucketPeers.size();
    }

    void addNewRandomPeerToKBucket() {
        std::scoped_lock lock(this->mutex);
        this->kbucketPeers.push_back(
            streamr::trackerlessnetwork::testutils::createMockPeerDescriptor());
    }

    folly::coro::Task<void> joinDht(
        std::vector<PeerDescriptor> /*entryPoints*/,
        bool /*doRandomJoin*/,
        bool /*retry*/) override {
        co_return;
    }

    folly::coro::Task<void> joinRing() override { co_return; }

    folly::coro::Task<void> start() override { co_return; }

    folly::coro::Task<void> stop() override { co_return; }
};

} // namespace streamr::trackerlessnetwork::testutils
