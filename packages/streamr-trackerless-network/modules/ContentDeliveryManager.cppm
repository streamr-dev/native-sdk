// Module streamr.trackerlessnetwork.ContentDeliveryManager
// Ported from packages/trackerless-network/src/ContentDeliveryManager.ts
// (v103.8.0-rc.3): per-stream-part join/leave/broadcast orchestration —
// creates a discovery-layer node (a layer-1 DhtNode over the layer-0
// control node), the content-delivery node, the entry-point store
// manager, split avoidance and reconnect for every joined stream part,
// and the ProxyClient for proxied stream parts.
//
// Adaptations: TS metrics (MetricsContext/RateMetric) and diagnostics
// (getDiagnosticInfo) are not ported (consistent with earlier phases).
// The TS private-client mode toggles on the ConnectionManager
// (enable/disable around proxied-only operation) have no C++
// counterpart yet — documented follow-up. The plumtree delivery options
// are milestone-E scope. The TS fire-and-forget setImmediate() join is
// a bounded GuardedAsyncScope task.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <vector>

export module streamr.trackerlessnetwork.ContentDeliveryManager;

import streamr.utils.CoroutineHelper;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;
import streamr.trackerlessnetwork.ContentDeliveryLayerNode;
import streamr.trackerlessnetwork.protos;
import streamr.trackerlessnetwork.ControlLayerNode;
import streamr.trackerlessnetwork.createContentDeliveryLayerNode;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.trackerlessnetwork.PeerDescriptorStoreManager;
import streamr.trackerlessnetwork.ProxyClient;
import streamr.trackerlessnetwork.StreamPartNetworkSplitAvoidance;
import streamr.trackerlessnetwork.StreamPartReconnect;
import streamr.trackerlessnetwork.streamPartIdToDataKey;
import streamr.dht.ConnectionLocker;
import streamr.dht.Identifiers;
import streamr.dht.Transport;
import streamr.dht.protos;
import streamr.utils.StreamID;
import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;
import streamr.utils.EthereumAddress;
import streamr.utils.StreamPartID;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using ::google::protobuf::Any;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::transport::Transport;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::EthereumAddress;
using streamr::utils::GuardedAsyncScope;
using streamr::utils::StreamID;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;

export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using streamr::trackerlessnetwork::controllayer::ControlLayerNode;
using streamr::trackerlessnetwork::controllayer::maxNodeCount;
using streamr::trackerlessnetwork::controllayer::PeerDescriptorStoreManager;
using streamr::trackerlessnetwork::controllayer::
    PeerDescriptorStoreManagerOptions;
using streamr::trackerlessnetwork::discoverylayer::DiscoveryLayerNode;
using streamr::trackerlessnetwork::proxy::ProxyClient;
using streamr::trackerlessnetwork::proxy::ProxyClientOptions;

namespace contentdeliverymanagerevents {

struct NewMessage : Event<StreamMessage> {};

} // namespace contentdeliverymanagerevents

using ContentDeliveryManagerEvents =
    std::tuple<contentdeliverymanagerevents::NewMessage>;

struct ContentDeliveryManagerOptions {
    std::optional<size_t> streamPartitionNeighborTargetCount;
    std::optional<size_t> streamPartitionMinPropagationTargets;
    std::optional<size_t> streamPartitionMaxPropagationBufferSize;
    std::optional<bool> acceptProxyConnections;
    std::optional<std::chrono::milliseconds> rpcRequestTimeout;
    std::optional<std::chrono::milliseconds> neighborUpdateInterval;
    // When true, a node's OWN published messages are not delivered back
    // to its local message listeners (no self-loop). Propagation to
    // neighbors and duplicate detection are unaffected. Off by default.
    bool suppressOwnMessageLoopback = false;
    // The layer-1 discovery node factory. TS constructs the DhtNode
    // inline; injected here because composing the DhtNode module graph
    // in this TU exhausts clang's source locations — use
    // createStreamPartDiscoveryLayerNode (its own module) as the
    // implementation.
    std::function<std::shared_ptr<discoverylayer::DiscoveryLayerNode>(
        const StreamPartID&, std::vector<::dht::PeerDescriptor>)>
        createDiscoveryLayerNode;
};

// TS StreamPartitionInfo (types.ts), minus the deprecated field.
struct StreamPartitionInfo {
    StreamPartID id;
    std::vector<PeerDescriptor> controlLayerNeighbors;
    std::vector<ContentDeliveryLayerNeighborInfo> contentDeliveryLayerNeighbors;
};

class ContentDeliveryManager
    : public EventEmitter<ContentDeliveryManagerEvents> {
public:
    // The TS StreamPartDelivery union: proxied=false carries the layer
    // objects, proxied=true carries the client. Public: the proxy
    // end-to-end tests (and diagnostics) inspect it via
    // getStreamPartDelivery().
    struct StreamPartDelivery {
        bool proxied = false;
        std::function<void(const StreamMessage&)> broadcast;
        std::function<folly::coro::Task<void>()> stop;
        // proxied == false:
        std::shared_ptr<DiscoveryLayerNode> discoveryLayerNode;
        std::shared_ptr<ContentDeliveryLayerNode> node;
        std::shared_ptr<PeerDescriptorStoreManager> peerDescriptorStoreManager;
        std::shared_ptr<StreamPartNetworkSplitAvoidance> networkSplitAvoidance;
        std::shared_ptr<StreamPartReconnect> streamPartReconnect;
        // proxied == true:
        std::shared_ptr<ProxyClient> client;
    };

private:
    ContentDeliveryManagerOptions options;
    ControlLayerNode* controlLayerNode = nullptr;
    Transport* transport = nullptr;
    ConnectionLocker* connectionLocker = nullptr;
    mutable std::recursive_mutex mutex;
    std::map<StreamPartID, std::shared_ptr<StreamPartDelivery>> streamParts;
    std::map<StreamPartID, std::vector<PeerDescriptor>>
        knownStreamPartEntryPoints;
    bool started = false;
    bool destroyed = false;
    // The TS setImmediate() joins; bounded by the dht join timeout.
    streamr::utils::SharedSerialExecutor joinExecutor{
        streamr::utils::SharedExecutors::worker()};
    GuardedAsyncScope joinScope;
    // Cancels the detached join/rejoin tasks at destroy(): the scope
    // contract requires the tasks to be cancelled BEFORE the drain,
    // otherwise destroy() waits for the joins to complete naturally — an
    // unbounded wait on worker-pool/network progress. Same mechanism and
    // fix as NetworkStack's joinCancellation; without it the CI
    // full-node teardown wedged in ContentDeliveryManager::destroy() ->
    // joinScope.close() one layer below the NetworkStack drain.
    folly::CancellationSource joinCancellation;

public:
    explicit ContentDeliveryManager(ContentDeliveryManagerOptions options)
        : options(std::move(options)) {}

    ~ContentDeliveryManager() override {
        streamr::utils::blockingWait(this->destroy());
    }

    folly::coro::Task<void> start(
        ControlLayerNode& startedAndJoinedControlLayerNode,
        Transport& transport, // NOLINT(bugprone-easily-swappable-parameters)
        ConnectionLocker& connectionLocker) {
        if (this->started || this->destroyed) {
            co_return;
        }
        this->started = true;
        this->controlLayerNode = &startedAndJoinedControlLayerNode;
        this->transport = &transport;
        this->connectionLocker = &connectionLocker;
        co_return;
    }

    folly::coro::Task<void> destroy() {
        {
            std::scoped_lock lock(this->mutex);
            if (!this->started || this->destroyed) {
                co_return;
            }
            SLogger::trace("Destroying ContentDeliveryManager");
            this->destroyed = true;
        }
        // Signal every part's split-avoidance and reconnect loops to
        // abort BEFORE joining the scheduled tasks: close() joins the
        // avoidNetworkSplit/reconnect coroutines, and without the abort
        // they run their full exponential backoff (~a minute per part)
        // before finishing.
        {
            std::scoped_lock lock(this->mutex);
            for (const auto& [id, part] : this->streamParts) {
                if (part->networkSplitAvoidance) {
                    part->networkSplitAvoidance->destroy();
                }
                if (part->streamPartReconnect) {
                    part->streamPartReconnect->destroy();
                }
            }
        }
        // Cancel the detached join/rejoin tasks, then drain with the
        // suspending closeAsync(): destroy() runs on a shared
        // blockingWait drive thread during test teardown, and a blocking
        // close() would both serialize sibling stops and wait unboundedly
        // for un-cancelled joins (the CI full-node teardown hang).
        this->joinCancellation.requestCancellation();
        co_await this->joinScope.closeAsync();
        std::vector<std::shared_ptr<StreamPartDelivery>> parts;
        {
            std::scoped_lock lock(this->mutex);
            for (const auto& [id, part] : this->streamParts) {
                parts.push_back(part);
            }
            this->streamParts.clear();
        }
        for (const auto& part : parts) {
            co_await part->stop();
        }
        this->removeAllListeners();
        this->controlLayerNode = nullptr;
        this->transport = nullptr;
        this->connectionLocker = nullptr;
    }

    void broadcast(const StreamMessage& msg) {
        const auto streamPartId = streamr::utils::toStreamPartID(
            StreamID{msg.messageid().streamid()},
            static_cast<uint32_t>(msg.messageid().streampartition()));
        SLogger::debug("Broadcasting to stream part " + streamPartId);
        this->joinStreamPart(streamPartId);
        std::shared_ptr<StreamPartDelivery> part;
        {
            std::scoped_lock lock(this->mutex);
            part = this->streamParts.at(streamPartId);
        }
        part->broadcast(msg);
    }

    folly::coro::Task<void> leaveStreamPart(StreamPartID streamPartId) {
        std::shared_ptr<StreamPartDelivery> part;
        {
            std::scoped_lock lock(this->mutex);
            const auto it = this->streamParts.find(streamPartId);
            if (it == this->streamParts.end()) {
                co_return;
            }
            part = it->second;
            this->streamParts.erase(it);
        }
        co_await part->stop();
    }

    void joinStreamPart(const StreamPartID& streamPartId) {
        {
            std::scoped_lock lock(this->mutex);
            if (this->streamParts.contains(streamPartId)) {
                return;
            }
        }
        SLogger::debug("Join stream part " + streamPartId);
        std::vector<PeerDescriptor> knownEntryPoints;
        {
            std::scoped_lock lock(this->mutex);
            const auto it = this->knownStreamPartEntryPoints.find(streamPartId);
            if (it != this->knownStreamPartEntryPoints.end()) {
                knownEntryPoints = it->second;
            }
        }
        auto discoveryLayerNode =
            this->createDiscoveryLayerNode(streamPartId, knownEntryPoints);
        auto peerDescriptorStoreManager =
            std::make_shared<PeerDescriptorStoreManager>(
                PeerDescriptorStoreManagerOptions{
                    .key = streamPartIdToDataKey(streamPartId),
                    .localPeerDescriptor = this->getPeerDescriptor(),
                    .fetchDataFromDht =
                        [this](DhtAddress key) {
                            return this->controlLayerNode->fetchDataFromDht(
                                std::move(key));
                        },
                    .storeDataToDht =
                        [this](DhtAddress key, Any data) {
                            return this->controlLayerNode->storeDataToDht(
                                std::move(key), std::move(data));
                        },
                    .deleteDataFromDht =
                        [this](DhtAddress key, bool waitForCompletion) {
                            return this->controlLayerNode->deleteDataFromDht(
                                std::move(key), waitForCompletion);
                        }});
        auto networkSplitAvoidance =
            std::make_shared<StreamPartNetworkSplitAvoidance>(
                StreamPartNetworkSplitAvoidanceOptions{
                    .discoveryLayerNode = *discoveryLayerNode,
                    .discoverEntryPoints = [peerDescriptorStoreManager]()
                        -> folly::coro::Task<std::vector<PeerDescriptor>> {
                        co_return co_await peerDescriptorStoreManager
                            ->fetchNodes();
                    }});
        auto node = this->createContentDeliveryLayerNode(
            streamPartId, discoveryLayerNode, [peerDescriptorStoreManager]() {
                return peerDescriptorStoreManager->isLocalNodeStored();
            });
        auto streamPartReconnect = std::make_shared<StreamPartReconnect>(
            *discoveryLayerNode, *peerDescriptorStoreManager);
        auto streamPart = std::make_shared<StreamPartDelivery>();
        streamPart->proxied = false;
        streamPart->discoveryLayerNode = discoveryLayerNode;
        streamPart->node = node;
        streamPart->peerDescriptorStoreManager = peerDescriptorStoreManager;
        streamPart->networkSplitAvoidance = networkSplitAvoidance;
        streamPart->streamPartReconnect = streamPartReconnect;
        streamPart->broadcast = [node](const StreamMessage& msg) {
            node->broadcast(msg);
        };
        streamPart->stop = [streamPartReconnect,
                            networkSplitAvoidance,
                            peerDescriptorStoreManager,
                            node,
                            discoveryLayerNode]() -> folly::coro::Task<void> {
            streamPartReconnect->destroy();
            networkSplitAvoidance->destroy();
            co_await peerDescriptorStoreManager->destroy();
            node->stop();
            co_await discoveryLayerNode->stop();
        };
        {
            std::scoped_lock lock(this->mutex);
            this->streamParts.emplace(streamPartId, streamPart);
        }
        node->on<contentdeliverylayernodeevents::Message>(
            [this](const StreamMessage& message) {
                this->emit<contentdeliverymanagerevents::NewMessage>(message);
            });
        discoveryLayerNode->on<
            streamr::trackerlessnetwork::discoverylayer::
                discoverylayernodeevents::ManualRejoinRequired>(
            [this, streamPartId, streamPartReconnect, networkSplitAvoidance]() {
                if (!streamPartReconnect->isRunning() &&
                    !networkSplitAvoidance->isRunning()) {
                    SLogger::debug(
                        "Manual rejoin required for stream part " +
                        streamPartId);
                    this->joinScope.add(
                        streamr::utils::co_withExecutor(
                            &this->joinExecutor,
                            streamr::utils::co_withCancellation(
                                this->joinCancellation.getToken(),
                                streamPartReconnect->reconnect())));
                }
            });
        node->on<contentdeliverylayernodeevents::EntryPointLeaveDetected>(
            [this, streamPartId, peerDescriptorStoreManager]() {
                this->joinScope.add(
                    streamr::utils::co_withExecutor(
                        &this->joinExecutor,
                        streamr::utils::co_withCancellation(
                            this->joinCancellation.getToken(),
                            this->handleEntryPointLeave(
                                streamPartId, peerDescriptorStoreManager))));
            });
        // TS setImmediate(): detached bounded join.
        this->joinScope.add(
            streamr::utils::co_withExecutor(
                &this->joinExecutor,
                streamr::utils::co_withCancellation(
                    this->joinCancellation.getToken(),
                    folly::coro::co_invoke(
                        [this, streamPartId, peerDescriptorStoreManager]()
                            -> folly::coro::Task<void> {
                            try {
                                co_await this->startLayersAndJoinDht(
                                    streamPartId, peerDescriptorStoreManager);
                            } catch (const std::exception& err) {
                                SLogger::warn(
                                    "Failed to join to stream part " +
                                    streamPartId + ": " +
                                    std::string(err.what()));
                            }
                        }))));
    }

    folly::coro::Task<void> setProxies(
        StreamPartID streamPartId,
        std::vector<PeerDescriptor> nodes,
        std::optional<ProxyDirection> direction,
        EthereumAddress userId,
        std::optional<size_t> connectionCount = std::nullopt) {
        // TS TODO preserved: explicit default value for
        // "acceptProxyConnections" or make it required.
        if (this->options.acceptProxyConnections.value_or(false)) {
            throw std::runtime_error(
                "cannot set proxies when acceptProxyConnections=true");
        }
        const bool enable = !nodes.empty() &&
            (!connectionCount.has_value() || connectionCount.value() > 0);
        if (enable) {
            std::shared_ptr<ProxyClient> client;
            bool alreadyProxied = false;
            {
                std::scoped_lock lock(this->mutex);
                alreadyProxied = this->isProxiedStreamPart(streamPartId);
                if (alreadyProxied) {
                    client = this->streamParts.at(streamPartId)->client;
                }
            }
            if (!alreadyProxied) {
                client = this->createProxyClient(streamPartId);
                auto streamPart = std::make_shared<StreamPartDelivery>();
                streamPart->proxied = true;
                streamPart->client = client;
                streamPart->broadcast = [client](const StreamMessage& msg) {
                    client->broadcast(msg);
                };
                streamPart->stop = [client]() -> folly::coro::Task<void> {
                    client->stop();
                    co_return;
                };
                {
                    std::scoped_lock lock(this->mutex);
                    this->streamParts.emplace(streamPartId, streamPart);
                }
                client->on<proxy::Message>(
                    [this](const StreamMessage& message) {
                        this->emit<contentdeliverymanagerevents::NewMessage>(
                            message);
                    });
                // TS enables ConnectionManager private-client mode when
                // every stream part is proxied; no C++ counterpart yet.
                client->start();
            }
            // ProxyClient::setProxies is synchronous (the shared-library
            // API); TS awaits and discards the result. Connection errors
            // are reported through the returned pair, which only the
            // shared library consumes.
            client->setProxies(nodes, direction, userId, connectionCount);
        } else {
            co_await this->leaveStreamPart(streamPartId);
        }
    }

    folly::coro::Task<bool> inspect(
        PeerDescriptor peerDescriptor, StreamPartID streamPartId) {
        std::shared_ptr<StreamPartDelivery> part;
        {
            std::scoped_lock lock(this->mutex);
            const auto it = this->streamParts.find(streamPartId);
            if (it != this->streamParts.end() && !it->second->proxied) {
                part = it->second;
            }
        }
        if (part) {
            co_return co_await part->node->inspect(std::move(peerDescriptor));
        }
        co_return false;
    }

    [[nodiscard]] std::vector<StreamPartitionInfo> getNodeInfo() const {
        std::scoped_lock lock(this->mutex);
        std::vector<StreamPartitionInfo> infos;
        for (const auto& [streamPartId, part] : this->streamParts) {
            if (part->proxied) {
                continue;
            }
            infos.push_back(
                StreamPartitionInfo{
                    .id = streamPartId,
                    .controlLayerNeighbors =
                        part->discoveryLayerNode->getNeighbors(),
                    .contentDeliveryLayerNeighbors = part->node->getInfos()});
        }
        return infos;
    }

    void setStreamPartEntryPoints(
        const StreamPartID& streamPartId,
        std::vector<PeerDescriptor> entryPoints) {
        std::scoped_lock lock(this->mutex);
        this->knownStreamPartEntryPoints[streamPartId] = std::move(entryPoints);
    }

    [[nodiscard]] bool isProxiedStreamPart(
        const StreamPartID& streamPartId,
        std::optional<ProxyDirection> direction = std::nullopt) const {
        std::scoped_lock lock(this->mutex);
        const auto it = this->streamParts.find(streamPartId);
        return it != this->streamParts.end() && it->second->proxied &&
            (!direction.has_value() ||
             it->second->client->getDirection() == direction.value());
    }

    [[nodiscard]] std::shared_ptr<StreamPartDelivery> getStreamPartDelivery(
        const StreamPartID& streamPartId) const {
        std::scoped_lock lock(this->mutex);
        const auto it = this->streamParts.find(streamPartId);
        return it != this->streamParts.end() ? it->second : nullptr;
    }

    [[nodiscard]] bool hasStreamPart(const StreamPartID& streamPartId) const {
        std::scoped_lock lock(this->mutex);
        return this->streamParts.contains(streamPartId);
    }

    [[nodiscard]] PeerDescriptor getPeerDescriptor() const {
        return this->controlLayerNode->getLocalPeerDescriptor();
    }

    [[nodiscard]] DhtAddress getNodeId() const {
        return Identifiers::getNodeIdFromPeerDescriptor(
            this->getPeerDescriptor());
    }

    [[nodiscard]] std::vector<DhtAddress> getNeighbors(
        const StreamPartID& streamPartId) const {
        std::shared_ptr<StreamPartDelivery> part;
        {
            std::scoped_lock lock(this->mutex);
            const auto it = this->streamParts.find(streamPartId);
            if (it == this->streamParts.end() || it->second->proxied) {
                return {};
            }
            part = it->second;
        }
        std::vector<DhtAddress> ids;
        for (const auto& descriptor : part->node->getNeighbors()) {
            ids.push_back(Identifiers::getNodeIdFromPeerDescriptor(descriptor));
        }
        return ids;
    }

    [[nodiscard]] std::vector<StreamPartID> getStreamParts() const {
        std::scoped_lock lock(this->mutex);
        std::vector<StreamPartID> ids;
        for (const auto& [id, part] : this->streamParts) {
            ids.push_back(id);
        }
        return ids;
    }

private:
    folly::coro::Task<void> handleEntryPointLeave(
        StreamPartID streamPartId,
        std::shared_ptr<PeerDescriptorStoreManager>
            peerDescriptorStoreManager) {
        {
            std::scoped_lock lock(this->mutex);
            if (this->destroyed ||
                peerDescriptorStoreManager->isLocalNodeStored() ||
                this->knownStreamPartEntryPoints.contains(streamPartId)) {
                co_return;
            }
        }
        const auto entryPoints =
            co_await peerDescriptorStoreManager->fetchNodes();
        if (entryPoints.size() < maxNodeCount) {
            co_await peerDescriptorStoreManager->storeAndKeepLocalNode();
        }
    }

    folly::coro::Task<void> startLayersAndJoinDht(
        StreamPartID streamPartId,
        std::shared_ptr<PeerDescriptorStoreManager>
            peerDescriptorStoreManager) {
        SLogger::debug(
            "Start layers and join DHT for stream part " + streamPartId);
        std::shared_ptr<StreamPartDelivery> streamPart;
        std::optional<std::vector<PeerDescriptor>> knownEntryPoints;
        {
            std::scoped_lock lock(this->mutex);
            const auto it = this->streamParts.find(streamPartId);
            if (it == this->streamParts.end() || it->second->proxied) {
                // leaveStreamPart has been called (or setProxies since).
                co_return;
            }
            streamPart = it->second;
            const auto epIt =
                this->knownStreamPartEntryPoints.find(streamPartId);
            if (epIt != this->knownStreamPartEntryPoints.end()) {
                knownEntryPoints = epIt->second;
            }
        }
        co_await streamPart->discoveryLayerNode->start();
        co_await streamPart->node->start();
        if (knownEntryPoints.has_value()) {
            co_await folly::coro::collectAll(
                streamPart->discoveryLayerNode->joinDht(
                    knownEntryPoints.value()),
                streamPart->discoveryLayerNode->joinRing());
        } else {
            auto entryPoints =
                co_await peerDescriptorStoreManager->fetchNodes();
            co_await folly::coro::collectAll(
                streamPart->discoveryLayerNode->joinDht(
                    sample(entryPoints, minNeighborCount)),
                streamPart->discoveryLayerNode->joinRing());
            if (entryPoints.size() < maxNodeCount) {
                co_await peerDescriptorStoreManager->storeAndKeepLocalNode();
                if (streamPart->discoveryLayerNode->getNeighborCount() <
                    minNeighborCount) {
                    auto networkSplitAvoidance =
                        streamPart->networkSplitAvoidance;
                    this->joinScope.add(
                        streamr::utils::co_withExecutor(
                            &this->joinExecutor,
                            streamr::utils::co_withCancellation(
                                this->joinCancellation.getToken(),
                                folly::coro::co_invoke(
                                    [networkSplitAvoidance]()
                                        -> folly::coro::Task<void> {
                                        co_await networkSplitAvoidance
                                            ->avoidNetworkSplit();
                                    }))));
                }
            }
        }
    }

    // lodash sampleSize: up to n random elements.
    static std::vector<PeerDescriptor> sample(
        std::vector<PeerDescriptor> input, size_t count) {
        static thread_local std::mt19937 generator{std::random_device{}()};
        std::shuffle(input.begin(), input.end(), generator);
        if (input.size() > count) {
            input.resize(count);
        }
        return input;
    }

    std::shared_ptr<DiscoveryLayerNode> createDiscoveryLayerNode(
        const StreamPartID& streamPartId,
        std::vector<PeerDescriptor> entryPoints) {
        return this->options.createDiscoveryLayerNode(
            streamPartId, std::move(entryPoints));
    }

    std::shared_ptr<ContentDeliveryLayerNode> createContentDeliveryLayerNode(
        const StreamPartID& streamPartId,
        std::shared_ptr<DiscoveryLayerNode> discoveryLayerNode,
        std::function<bool()> isLocalNodeEntryPoint) {
        return streamr::trackerlessnetwork::createContentDeliveryLayerNode(
            ContentDeliveryLayerNodeOptions{
                .streamPartId = streamPartId,
                .discoveryLayerNode = std::move(discoveryLayerNode),
                .transport = this->transport,
                .connectionLocker = this->connectionLocker,
                .localPeerDescriptor =
                    this->controlLayerNode->getLocalPeerDescriptor(),
                .isLocalNodeEntryPoint = std::move(isLocalNodeEntryPoint),
                .neighborTargetCount =
                    this->options.streamPartitionNeighborTargetCount,
                .minPropagationTargets =
                    this->options.streamPartitionMinPropagationTargets,
                .maxPropagationBufferSize =
                    this->options.streamPartitionMaxPropagationBufferSize,
                .acceptProxyConnections = this->options.acceptProxyConnections,
                .neighborUpdateInterval = this->options.neighborUpdateInterval,
                .rpcRequestTimeout = this->options.rpcRequestTimeout,
                .suppressOwnMessageLoopback =
                    this->options.suppressOwnMessageLoopback});
    }

    std::shared_ptr<ProxyClient> createProxyClient(
        const StreamPartID& streamPartId) {
        return std::make_shared<ProxyClient>(ProxyClientOptions{
            .transport = *this->transport,
            .localPeerDescriptor =
                this->controlLayerNode->getLocalPeerDescriptor(),
            .streamPartId = streamPartId,
            .connectionLocker = *this->connectionLocker,
            .minPropagationTargets =
                this->options.streamPartitionMinPropagationTargets});
    }
};

} // namespace streamr::trackerlessnetwork
