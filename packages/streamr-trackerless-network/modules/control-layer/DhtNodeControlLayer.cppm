// Module streamr.trackerlessnetwork.DhtNodeControlLayer
// The TS code passes a layer-0 DhtNode wherever a ControlLayerNode is
// expected (structural typing); this adapter provides that shape
// nominally over streamr::dht::DhtNode, mirroring DhtNodeDiscoveryLayer.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <memory>
#include <vector>

export module streamr.trackerlessnetwork.DhtNodeControlLayer;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ControlLayerNode;
import streamr.dht.ConnectionsView;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.Transport;
import streamr.dht.protos;

using streamr::dht::DhtAddress;
using streamr::dht::DhtNode;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::transport::Transport;

export namespace streamr::trackerlessnetwork::controllayer {

class DhtNodeControlLayer : public ControlLayerNode {
private:
    std::shared_ptr<DhtNode> dhtNode;

public:
    explicit DhtNodeControlLayer(std::shared_ptr<DhtNode> dhtNode)
        : dhtNode(std::move(dhtNode)) {}

    [[nodiscard]] DhtNode& getDhtNode() { return *this->dhtNode; }

    folly::coro::Task<void> joinDht(
        std::vector<PeerDescriptor> entryPointDescriptors) override {
        co_await this->dhtNode->joinDht(std::move(entryPointDescriptors));
    }

    [[nodiscard]] bool hasJoined() override {
        return this->dhtNode->hasJoined();
    }

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() override {
        return this->dhtNode->getLocalPeerDescriptor();
    }

    folly::coro::Task<std::vector<DataEntry>> fetchDataFromDht(
        DhtAddress key) override {
        co_return co_await this->dhtNode->fetchDataFromDht(key);
    }

    folly::coro::Task<std::vector<PeerDescriptor>> storeDataToDht(
        DhtAddress key, Any data) override {
        co_return co_await this->dhtNode->storeDataToDht(
            std::move(key), std::move(data));
    }

    folly::coro::Task<void> deleteDataFromDht(
        DhtAddress key, bool waitForCompletion) override {
        co_await this->dhtNode->deleteDataFromDht(
            std::move(key), waitForCompletion);
    }

    folly::coro::Task<void> waitForNetworkConnectivity() override {
        co_await this->dhtNode->waitForNetworkConnectivity();
    }

    [[nodiscard]] Transport* asTransport() override {
        return this->dhtNode.get();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getNeighbors() override {
        return this->dhtNode->getNeighbors();
    }

    [[nodiscard]] ConnectionsView* getConnectionsView() override {
        return this->dhtNode->getConnectionsView();
    }

    folly::coro::Task<void> start() override {
        co_await this->dhtNode->start();
    }

    folly::coro::Task<void> stop() override {
        this->dhtNode->stop();
        co_return;
    }
};

} // namespace streamr::trackerlessnetwork::controllayer
