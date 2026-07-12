// Module streamr.trackerlessnetwork.NeighborUpdateManager
// Ported from packages/trackerless-network/src/content-delivery-layer/
// neighbor-discovery/NeighborUpdateManager.ts (v103.8.0-rc.3): sends the
// local neighbor list to every neighbor at a fixed interval, records the
// round-trip time, and drops neighbors that ask to be removed.
//
// Adaptation: the TS `await scheduleAtInterval(...)` becomes a bounded
// task on a GuardedAsyncScope (each round is a set of RPCs with
// timeouts, and stop() aborts the interval before draining the scope).
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <memory>
#include <set>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.NeighborUpdateManager;

import streamr.utils.CoroutineHelper;
import streamr.utils.AbortController;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.scheduleAtInterval;
import streamr.utils.SharedExecutors;
import streamr.trackerlessnetwork.NeighborFinder;
import streamr.trackerlessnetwork.NeighborUpdateRpcLocal;
import streamr.trackerlessnetwork.NeighborUpdateRpcRemote;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.NodeList;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;
import streamr.logger.SLogger;
import streamr.utils.StreamPartID;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::logger::SLogger;
using streamr::utils::AbortController;
using streamr::utils::GuardedAsyncScope;
using streamr::utils::scheduleAtInterval;
using streamr::utils::SharedSerialExecutor;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork::neighbordiscovery {

using ::dht::PeerDescriptor;

constexpr std::chrono::milliseconds defaultNeighborUpdateInterval{10000};

struct NeighborUpdateManagerOptions {
    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    NodeList& neighbors;
    NodeList& nearbyNodeView;
    INeighborFinder& neighborFinder;
    ListeningRpcCommunicator& rpcCommunicator;
    std::chrono::milliseconds neighborUpdateInterval;
    size_t neighborTargetCount;
    std::set<DhtAddress>& ongoingHandshakes;
};

class NeighborUpdateManager {
private:
    NeighborUpdateManagerOptions options;
    NeighborUpdateRpcLocal rpcLocal;
    AbortController abortController;
    SharedSerialExecutor executor{streamr::utils::SharedExecutors::worker()};
    GuardedAsyncScope scope;

public:
    explicit NeighborUpdateManager(NeighborUpdateManagerOptions options)
        : options(options), // NOLINT(performance-unnecessary-value-param)
          rpcLocal(
              NeighborUpdateRpcLocalOptions{
                  .localPeerDescriptor = options.localPeerDescriptor,
                  .streamPartId = options.streamPartId,
                  .neighbors = options.neighbors,
                  .nearbyNodeView = options.nearbyNodeView,
                  .neighborFinder = options.neighborFinder,
                  .rpcCommunicator = options.rpcCommunicator,
                  .neighborTargetCount = options.neighborTargetCount,
                  .ongoingHandshakes = options.ongoingHandshakes}) {
        this->options.rpcCommunicator
            .registerRpcMethod<NeighborUpdate, NeighborUpdate>(
                "neighborUpdate",
                [this](
                    const NeighborUpdate& request,
                    const DhtCallContext& context) {
                    return this->rpcLocal.neighborUpdate(request, context);
                });
    }

    ~NeighborUpdateManager() { this->stop(); }

    void start() {
        this->scope.add(
            streamr::utils::co_withExecutor(
                &this->executor,
                folly::coro::co_invoke([this]() -> folly::coro::Task<void> {
                    co_await scheduleAtInterval(
                        [this]() -> folly::coro::Task<void> {
                            co_await this->updateNeighborInfo();
                        },
                        this->options.neighborUpdateInterval,
                        false,
                        this->abortController.getSignal(),
                        &this->executor);
                })));
    }

    void stop() {
        this->abortController.abort();
        this->scope.close();
    }

private:
    folly::coro::Task<void> updateNeighborInfo() {
        SLogger::trace("Updating neighbor info to nodes");
        std::vector<PeerDescriptor> neighborDescriptors;
        for (const auto& neighbor : this->options.neighbors.getAll()) {
            neighborDescriptors.push_back(neighbor->getPeerDescriptor());
        }
        const auto startTime = std::chrono::steady_clock::now();
        std::vector<folly::coro::Task<void>> updates;
        for (const auto& neighbor : this->options.neighbors.getAll()) {
            updates.push_back(this->updateNeighbor(
                neighbor->getPeerDescriptor(), neighborDescriptors, startTime));
        }
        co_await folly::coro::collectAllTryRange(std::move(updates));
    }

    folly::coro::Task<void> updateNeighbor(
        PeerDescriptor peerDescriptor,
        std::vector<PeerDescriptor> neighborDescriptors,
        std::chrono::steady_clock::time_point startTime) {
        const auto response =
            co_await this->createRemote(peerDescriptor)
                ->updateNeighbors(
                    this->options.streamPartId, std::move(neighborDescriptors));
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        const auto neighbor = this->options.neighbors.get(nodeId);
        if (neighbor.has_value()) {
            neighbor.value()->setRtt(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime)
                    .count());
        }
        if (response.removeMe) {
            this->options.neighbors.remove(nodeId);
            this->options.neighborFinder.start({nodeId});
        }
    }

    std::shared_ptr<NeighborUpdateRpcRemote> createRemote(
        const PeerDescriptor& targetPeerDescriptor) {
        NeighborUpdateRpcClient client{this->options.rpcCommunicator};
        return std::make_shared<NeighborUpdateRpcRemote>(
            this->options.localPeerDescriptor, targetPeerDescriptor, client);
    }
};

} // namespace streamr::trackerlessnetwork::neighbordiscovery
