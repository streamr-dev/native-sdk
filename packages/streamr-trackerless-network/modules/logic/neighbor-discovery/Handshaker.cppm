// Module streamr.trackerlessnetwork.Handshaker
// Ported from packages/trackerless-network/src/content-delivery-layer/
// neighbor-discovery/Handshaker.ts (v103.8.0-rc.3): selects handshake
// targets from the four contact views (ring left/right, Kademlia-nearby,
// random), runs up to two handshakes in parallel, and handles the
// interleaving protocol via HandshakeRpcLocal.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <algorithm>
#include <chrono>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.Handshaker;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.HandshakeRpcLocal;
import streamr.trackerlessnetwork.HandshakeRpcRemote;
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
using streamr::trackerlessnetwork::ContentDeliveryRpcRemote;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork::neighbordiscovery {

using ::dht::PeerDescriptor;
using streamr::trackerlessnetwork::ContentDeliveryRpcClient;

constexpr size_t parallelHandshakeCount = 2;

struct HandshakerOptions {
    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    NodeList& neighbors;
    NodeList& leftNodeView;
    NodeList& rightNodeView;
    NodeList& nearbyNodeView;
    NodeList& randomNodeView;
    ListeningRpcCommunicator& rpcCommunicator;
    size_t maxNeighborCount;
    std::set<DhtAddress>& ongoingHandshakes;
    std::optional<std::chrono::milliseconds> rpcRequestTimeout = std::nullopt;
};

class Handshaker {
private:
    HandshakerOptions options;
    std::set<DhtAddress> ongoingInterleaves;
    HandshakeRpcLocal rpcLocal;

public:
    explicit Handshaker(HandshakerOptions options)
        : options(std::move(options)),
          rpcLocal(
              HandshakeRpcLocalOptions{
                  .streamPartId = this->options.streamPartId,
                  .neighbors = this->options.neighbors,
                  .ongoingHandshakes = this->options.ongoingHandshakes,
                  .ongoingInterleaves = this->ongoingInterleaves,
                  .maxNeighborCount = this->options.maxNeighborCount,
                  .createRpcRemote =
                      [this](const PeerDescriptor& target) {
                          return this->createRpcRemote(target);
                      },
                  .createContentDeliveryRpcRemote =
                      [this](const PeerDescriptor& target) {
                          return this->createContentDeliveryRpcRemote(target);
                      },
                  .handshakeWithInterleaving =
                      [this](PeerDescriptor target, DhtAddress remoteNodeId)
                      -> folly::coro::Task<bool> {
                      return this->handshakeWithInterleaving(
                          std::move(target), std::move(remoteNodeId));
                  }}) {
        this->options.rpcCommunicator
            .registerRpcMethodAsync<InterleaveRequest, InterleaveResponse>(
                "interleaveRequest",
                [this](InterleaveRequest request, DhtCallContext context) {
                    return this->rpcLocal.interleaveRequest(
                        std::move(request), std::move(context));
                },
                {.timeout =
                     static_cast<size_t>(interleaveRequestTimeout.count())});
        this->options.rpcCommunicator.registerRpcMethod<
            StreamPartHandshakeRequest,
            StreamPartHandshakeResponse>(
            "handshake",
            [this](
                const StreamPartHandshakeRequest& request,
                const DhtCallContext& context) {
                return this->rpcLocal.handshake(request, context);
            });
    }

    folly::coro::Task<std::vector<DhtAddress>> attemptHandshakesOnContacts(
        std::vector<DhtAddress> excludedIds) {
        // TS TODO preserved: use an options option or named constant?
        // or why the value 2?
        if (this->options.neighbors.size() +
                this->options.ongoingHandshakes.size() <
            this->options.maxNeighborCount - 2) {
            SLogger::trace("Attempting parallel handshakes with 2 targets");
            co_return co_await this->selectParallelTargetsAndHandshake(
                std::move(excludedIds));
        }
        if (this->options.neighbors.size() +
                this->options.ongoingHandshakes.size() <
            this->options.maxNeighborCount) {
            SLogger::trace("Attempting handshake with new target");
            co_return co_await this->selectNewTargetAndHandshake(
                std::move(excludedIds));
        }
        co_return excludedIds;
    }

    [[nodiscard]] const std::set<DhtAddress>& getOngoingHandshakes() const {
        return this->options.ongoingHandshakes;
    }

private:
    folly::coro::Task<std::vector<DhtAddress>>
    selectParallelTargetsAndHandshake(std::vector<DhtAddress> excludedIds) {
        auto exclude = excludedIds;
        for (const auto& id : this->options.neighbors.getIds()) {
            exclude.push_back(id);
        }
        const auto targets = this->selectParallelTargets(exclude);
        for (const auto& target : targets) {
            this->options.ongoingHandshakes.insert(
                Identifiers::getNodeIdFromPeerDescriptor(
                    target->getPeerDescriptor()));
        }
        co_return co_await this->doParallelHandshakes(
            targets, std::move(exclude));
    }

    std::vector<std::shared_ptr<HandshakeRpcRemote>> selectParallelTargets(
        const std::vector<DhtAddress>& excludedIds) {
        // Insertion-ordered and deduplicated by node id (the TS version
        // uses a Map for the same effect).
        std::vector<
            std::pair<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>>>
            targets;
        const auto getExcludedIds = [&excludedIds, &targets]() {
            auto ids = excludedIds;
            for (const auto& [id, remote] : targets) {
                ids.push_back(id);
            }
            return ids;
        };
        const auto addTarget =
            [&targets](
                const std::optional<std::shared_ptr<ContentDeliveryRpcRemote>>&
                    candidate) {
                if (!candidate.has_value()) {
                    return;
                }
                const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
                    candidate.value()->getPeerDescriptor());
                const auto isKnown =
                    std::ranges::any_of(targets, [&nodeId](const auto& pair) {
                        return pair.first == nodeId;
                    });
                if (!isKnown) {
                    targets.emplace_back(nodeId, candidate.value());
                }
            };

        // Step 1: If no neighbors, try to find a WebSocket node first
        if (this->options.neighbors.size() == 0) {
            addTarget(
                this->options.nearbyNodeView.getFirst(getExcludedIds(), true));
        }
        // Step 2: Add left and right contacts from the ring
        addTarget(this->options.leftNodeView.getFirst(getExcludedIds()));
        addTarget(this->options.rightNodeView.getFirst(getExcludedIds()));
        // Step 3: Add closest contact based on Kademlia metric if needed
        if (targets.size() < parallelHandshakeCount) {
            addTarget(this->options.nearbyNodeView.getFirst(getExcludedIds()));
        }
        // Step 4: Fill remaining slots with random contacts
        while (targets.size() < parallelHandshakeCount) {
            const auto random =
                this->options.randomNodeView.getRandom(getExcludedIds());
            if (!random.has_value()) {
                break;
            }
            addTarget(random);
        }

        std::vector<std::shared_ptr<HandshakeRpcRemote>> remotes;
        remotes.reserve(targets.size());
        for (const auto& [id, neighbor] : targets) {
            remotes.push_back(
                this->createRpcRemote(neighbor->getPeerDescriptor()));
        }
        return remotes;
    }

    folly::coro::Task<std::vector<DhtAddress>> doParallelHandshakes(
        std::vector<std::shared_ptr<HandshakeRpcRemote>> targets,
        std::vector<DhtAddress> excludedIds) {
        std::vector<folly::coro::Task<bool>> handshakes;
        handshakes.reserve(targets.size());
        for (size_t i = 0; i < targets.size(); i++) {
            const auto& otherNode = (i == 0)
                ? ((targets.size() > 1) ? targets[1] : nullptr)
                : targets[0];
            // TS TODO preserved: better check (currently this condition
            // is always true)
            std::optional<DhtAddress> otherNodeId = otherNode
                ? std::optional(
                      Identifiers::getNodeIdFromPeerDescriptor(
                          otherNode->getPeerDescriptor()))
                : std::nullopt;
            handshakes.push_back(
                this->handshakeWithTarget(targets[i], otherNodeId));
        }
        const auto results =
            co_await folly::coro::collectAllTryRange(std::move(handshakes));
        for (size_t i = 0; i < results.size(); i++) {
            if (!results[i].hasValue() || !results[i].value()) {
                excludedIds.push_back(
                    Identifiers::getNodeIdFromPeerDescriptor(
                        targets[i]->getPeerDescriptor()));
            }
        }
        co_return excludedIds;
    }

    folly::coro::Task<std::vector<DhtAddress>> selectNewTargetAndHandshake(
        std::vector<DhtAddress> excludedIds) {
        auto exclude = excludedIds;
        for (const auto& id : this->options.neighbors.getIds()) {
            exclude.push_back(id);
        }
        auto target = this->options.leftNodeView.getFirst(exclude);
        if (!target.has_value()) {
            target = this->options.rightNodeView.getFirst(exclude);
        }
        if (!target.has_value()) {
            target = this->options.nearbyNodeView.getFirst(exclude);
        }
        if (!target.has_value()) {
            target = this->options.randomNodeView.getRandom(exclude);
        }
        if (target.has_value()) {
            const auto accepted = co_await this->handshakeWithTarget(
                this->createRpcRemote(target.value()->getPeerDescriptor()),
                std::nullopt);
            if (!accepted) {
                excludedIds.push_back(
                    Identifiers::getNodeIdFromPeerDescriptor(
                        target.value()->getPeerDescriptor()));
            }
        }
        co_return excludedIds;
    }

    folly::coro::Task<bool> handshakeWithTarget(
        std::shared_ptr<HandshakeRpcRemote> target,
        std::optional<DhtAddress> concurrentNodeId) {
        const auto targetNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            target->getPeerDescriptor());
        this->options.ongoingHandshakes.insert(targetNodeId);
        const auto result = co_await target->handshake(
            this->options.streamPartId,
            this->options.neighbors.getIds(),
            concurrentNodeId);
        if (result.accepted) {
            this->options.neighbors.add(this->createContentDeliveryRpcRemote(
                target->getPeerDescriptor()));
        }
        if (result.interleaveTargetDescriptor.has_value()) {
            co_await this->handshakeWithInterleaving(
                result.interleaveTargetDescriptor.value(), targetNodeId);
        }
        this->options.ongoingHandshakes.erase(targetNodeId);
        co_return result.accepted;
    }

    folly::coro::Task<bool> handshakeWithInterleaving(
        PeerDescriptor target, DhtAddress remoteNodeId) {
        const auto remote = this->createRpcRemote(target);
        const auto targetNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            remote->getPeerDescriptor());
        this->options.ongoingHandshakes.insert(targetNodeId);
        const auto result = co_await remote->handshake(
            this->options.streamPartId,
            this->options.neighbors.getIds(),
            std::nullopt,
            remoteNodeId);
        if (result.accepted) {
            this->options.neighbors.add(this->createContentDeliveryRpcRemote(
                remote->getPeerDescriptor()));
        }
        this->options.ongoingHandshakes.erase(targetNodeId);
        co_return result.accepted;
    }

    std::shared_ptr<HandshakeRpcRemote> createRpcRemote(
        const PeerDescriptor& targetPeerDescriptor) {
        HandshakeRpcClient client{this->options.rpcCommunicator};
        return std::make_shared<HandshakeRpcRemote>(
            this->options.localPeerDescriptor,
            targetPeerDescriptor,
            client,
            this->options.rpcRequestTimeout);
    }

    std::shared_ptr<ContentDeliveryRpcRemote> createContentDeliveryRpcRemote(
        const PeerDescriptor& targetPeerDescriptor) {
        ContentDeliveryRpcClient client{this->options.rpcCommunicator};
        return std::make_shared<ContentDeliveryRpcRemote>(
            this->options.localPeerDescriptor,
            targetPeerDescriptor,
            client,
            this->options.rpcRequestTimeout);
    }
};

} // namespace streamr::trackerlessnetwork::neighbordiscovery
