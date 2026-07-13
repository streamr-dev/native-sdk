// Module streamr.trackerlessnetwork.NetworkStack
// Ported from packages/trackerless-network/src/NetworkStack.ts
// (v103.8.0-rc.3): the layer-0 DhtNode + ContentDeliveryManager
// composition with lifecycle and the node-info RPC service.
//
// Adaptations: the TS module-level instances list and process/window
// exit handlers are a Node.js/browser runtime concern and are not
// ported — C++ callers own the stack's lifetime (the shared library has
// its own teardown). MetricsContext is not ported (consistent with
// earlier phases). The TS setImmediate() fire-and-forget joinDht runs
// as a bounded GuardedAsyncScope task. The manager's discovery-layer
// factory (injected in C5 because the DhtNode module graph cannot be
// composed inside ContentDeliveryManager.cppm) is supplied here with
// createStreamPartDiscoveryLayerNode, matching the TS inline
// construction.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module streamr.trackerlessnetwork.NetworkStack;

import streamr.utils.CoroutineHelper;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;
import streamr.utils.StreamID;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;
import streamr.trackerlessnetwork.protos;
import streamr.trackerlessnetwork.ContentDeliveryManager;
import streamr.trackerlessnetwork.ControlLayerNode;
import streamr.trackerlessnetwork.DhtNodeControlLayer;
import streamr.trackerlessnetwork.createStreamPartDiscoveryLayerNode;
import streamr.trackerlessnetwork.NodeInfoClient;
import streamr.trackerlessnetwork.NodeInfoRpcLocal;
import streamr.dht.ConnectionLocker;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.Transport;
import streamr.dht.Version;
import streamr.dht.protos;
import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using streamr::dht::DhtNode;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::helpers::Version;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::logger::SLogger;
using streamr::utils::GuardedAsyncScope;
using streamr::utils::StreamID;
using streamr::utils::StreamPartID;
using streamr::utils::waitForCondition;

export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using streamr::trackerlessnetwork::controllayer::ControlLayerNode;
using streamr::trackerlessnetwork::controllayer::
    createStreamPartDiscoveryLayerNode;
using streamr::trackerlessnetwork::controllayer::DhtNodeControlLayer;

// TS NetworkOptions, minus metricsContext (not ported).
struct NetworkOptions {
    streamr::dht::DhtNodeOptions layer0;
    ContentDeliveryManagerOptions networkNode;
};

// TS joinStreamPart neighborRequirement parameter.
struct NeighborRequirement {
    size_t minCount;
    std::chrono::milliseconds timeout;
};

class NetworkStack {
private:
    NetworkOptions options;
    std::shared_ptr<DhtNode> dhtNode;
    std::shared_ptr<DhtNodeControlLayer> controlLayerNode;
    std::shared_ptr<ContentDeliveryManager> contentDeliveryManager;
    std::unique_ptr<ListeningRpcCommunicator> infoRpcCommunicator;
    std::unique_ptr<NodeInfoRpcLocal> nodeInfoRpcLocal;
    std::unique_ptr<NodeInfoClient> nodeInfoClient;
    bool stopped = false;
    // The TS setImmediate() joinDht; bounded by the dht join timeout.
    streamr::utils::SharedSerialExecutor joinExecutor{
        streamr::utils::SharedExecutors::worker()};
    GuardedAsyncScope joinScope;

    folly::coro::Task<void> ensureConnectedToControlLayer() {
        // TS TODO preserved: could wrap joinDht with pOnce and call it
        // here.
        if (!this->controlLayerNode->hasJoined()) {
            if (!this->options.layer0.entryPoints.empty()) {
                // TS TODO preserved: should catch possible rejection?
                this->joinScope.add(
                    streamr::utils::co_withExecutor(
                        &this->joinExecutor,
                        folly::coro::co_invoke(
                            [this]() -> folly::coro::Task<void> {
                                co_await this->controlLayerNode->joinDht(
                                    this->options.layer0.entryPoints);
                            })));
            }
            co_await this->controlLayerNode->waitForNetworkConnectivity();
        }
    }

public:
    explicit NetworkStack(NetworkOptions options)
        : options(std::move(options)) {
        auto layer0 = this->options.layer0;
        // TS: allowIncomingPrivateConnections =
        // options.networkNode?.acceptProxyConnections.
        layer0.allowIncomingPrivateConnections =
            this->options.networkNode.acceptProxyConnections.value_or(false);
        this->dhtNode = std::make_shared<DhtNode>(std::move(layer0));
        this->controlLayerNode =
            std::make_shared<DhtNodeControlLayer>(this->dhtNode);
        auto managerOptions = this->options.networkNode;
        if (!managerOptions.createDiscoveryLayerNode) {
            // The TS inline layer-1 DhtNode construction; see the module
            // comment for why the manager takes this as a factory.
            managerOptions.createDiscoveryLayerNode =
                [this](
                    const StreamPartID& streamPartId,
                    std::vector<PeerDescriptor> entryPoints) {
                    return createStreamPartDiscoveryLayerNode(
                        streamPartId,
                        std::move(entryPoints),
                        *this->controlLayerNode);
                };
        }
        this->contentDeliveryManager =
            std::make_shared<ContentDeliveryManager>(std::move(managerOptions));
    }

    NetworkStack(const NetworkStack&) = delete;
    NetworkStack& operator=(const NetworkStack&) = delete;
    NetworkStack(NetworkStack&&) = delete;
    NetworkStack& operator=(NetworkStack&&) = delete;

    virtual ~NetworkStack() { streamr::utils::blockingWait(this->stop()); }

    folly::coro::Task<void> start(bool doJoin = true) {
        SLogger::info("Starting a Streamr Network Node");
        co_await this->controlLayerNode->start();
        SLogger::info(
            "Node id is " +
            Identifiers::getNodeIdFromPeerDescriptor(
                this->controlLayerNode->getLocalPeerDescriptor()));
        const auto localPeerDescriptor =
            this->controlLayerNode->getLocalPeerDescriptor();
        bool localIsEntryPoint = false;
        for (const auto& entryPoint : this->options.layer0.entryPoints) {
            if (Identifiers::areEqualPeerDescriptors(
                    entryPoint, localPeerDescriptor)) {
                localIsEntryPoint = true;
                break;
            }
        }
        if (localIsEntryPoint) {
            co_await this->controlLayerNode->joinDht(
                this->options.layer0.entryPoints);
        } else if (doJoin) {
            // In practice there aren't existing connections and
            // therefore this always connects (TS comment).
            co_await this->ensureConnectedToControlLayer();
        }
        // TS casts the transport to ConnectionManager and passes it as
        // both the transport and the locker; here the locker is either
        // the one the node resolved (owned ConnectionManager or the
        // explicitly given one) or the transport itself (the simulator
        // transport implements ConnectionLocker).
        auto* transport = this->dhtNode->getTransport();
        auto* connectionLocker = this->dhtNode->getConnectionLocker();
        if (connectionLocker == nullptr) {
            connectionLocker = dynamic_cast<ConnectionLocker*>(transport);
        }
        if (connectionLocker == nullptr) {
            throw std::runtime_error(
                "NetworkStack: the layer-0 transport does not provide a ConnectionLocker");
        }
        co_await this->contentDeliveryManager->start(
            *this->controlLayerNode, *transport, *connectionLocker);
        this->infoRpcCommunicator = std::make_unique<ListeningRpcCommunicator>(
            ServiceID{nodeInfoRpcServiceId}, *this->dhtNode->getTransport());
        this->nodeInfoRpcLocal = std::make_unique<NodeInfoRpcLocal>(
            [this]() { return this->createNodeInfo(); },
            *this->infoRpcCommunicator);
        this->nodeInfoClient = std::make_unique<NodeInfoClient>(
            this->controlLayerNode->getLocalPeerDescriptor(),
            *this->infoRpcCommunicator);
    }

    // virtual so the NetworkNode unit test can stub the stack (the TS
    // test passes a partial object).
    virtual folly::coro::Task<void> joinStreamPart(
        StreamPartID streamPartId,
        std::optional<NeighborRequirement> neighborRequirement = std::nullopt) {
        if (this->getContentDeliveryManager().isProxiedStreamPart(
                streamPartId)) {
            throw std::runtime_error(
                "Cannot join to " + streamPartId +
                " as proxy connections have been set");
        }
        co_await this->ensureConnectedToControlLayer();
        this->getContentDeliveryManager().joinStreamPart(streamPartId);
        if (neighborRequirement.has_value()) {
            co_await waitForCondition(
                [this, streamPartId, &neighborRequirement]() {
                    return this->getContentDeliveryManager()
                               .getNeighbors(streamPartId)
                               .size() >= neighborRequirement->minCount;
                },
                neighborRequirement->timeout);
        }
    }

    folly::coro::Task<void> broadcast(const StreamMessage& msg) {
        const auto streamPartId = streamr::utils::toStreamPartID(
            StreamID{msg.messageid().streamid()},
            static_cast<uint32_t>(msg.messageid().streampartition()));
        if (this->getContentDeliveryManager().isProxiedStreamPart(
                streamPartId, ProxyDirection::SUBSCRIBE) &&
            msg.has_contentmessage()) {
            throw std::runtime_error(
                "Cannot broadcast to " + streamPartId +
                " as proxy subscribe connections have been set");
        }
        // TS TODO preserved: could combine these two calls to
        // isProxiedStreamPart?
        if (!this->contentDeliveryManager->isProxiedStreamPart(streamPartId)) {
            co_await this->ensureConnectedToControlLayer();
        }
        this->getContentDeliveryManager().broadcast(msg);
    }

    virtual ContentDeliveryManager& getContentDeliveryManager() {
        return *this->contentDeliveryManager;
    }

    [[nodiscard]] ControlLayerNode& getControlLayerNode() {
        return *this->controlLayerNode;
    }

    // The transport the layer-0 node runs on (TS
    // controlLayerNode.getTransport(), a ConnectionManager on the
    // own-connection-manager path).
    [[nodiscard]] Transport* getTransport() {
        return this->dhtNode->getTransport();
    }

    folly::coro::Task<NodeInfoResponse> fetchNodeInfo(PeerDescriptor node) {
        if (!Identifiers::areEqualPeerDescriptors(
                node, this->getControlLayerNode().getLocalPeerDescriptor())) {
            co_return co_await this->nodeInfoClient->getInfo(std::move(node));
        }
        co_return this->createNodeInfo();
    }

    NodeInfoResponse createNodeInfo() {
        NodeInfoResponse response;
        *response.mutable_peerdescriptor() =
            this->getControlLayerNode().getLocalPeerDescriptor();
        auto* controlLayer = response.mutable_controllayer();
        for (const auto& connection : this->getControlLayerNode()
                                          .getConnectionsView()
                                          ->getConnections()) {
            *controlLayer->add_connections() = connection;
        }
        for (const auto& neighbor :
             this->getControlLayerNode().getNeighbors()) {
            *controlLayer->add_neighbors() = neighbor;
        }
        for (const auto& info :
             this->getContentDeliveryManager().getNodeInfo()) {
            auto* partition = response.add_streampartitions();
            partition->set_id(info.id);
            for (const auto& neighbor : info.controlLayerNeighbors) {
                *partition->add_controllayerneighbors() = neighbor;
            }
            for (const auto& neighbor : info.contentDeliveryLayerNeighbors) {
                auto* neighborInfo =
                    partition->add_contentdeliverylayerneighbors();
                *neighborInfo->mutable_peerdescriptor() =
                    neighbor.peerDescriptor;
                if (neighbor.rtt.has_value()) {
                    neighborInfo->set_rtt(
                        static_cast<int32_t>(neighbor.rtt.value()));
                }
            }
        }
        // TS reports the trackerless-network package version; the C++
        // port reports the SDK application version.
        response.set_applicationversion(Version::localApplicationVersion);
        return response;
    }

    [[nodiscard]] const NetworkOptions& getOptions() const {
        return this->options;
    }

    folly::coro::Task<void> stop() {
        if (!this->stopped) {
            this->stopped = true;
            // Drain the fire-and-forget joins before the members die
            // (teardown ordering; see ContentDeliveryManager::destroy).
            this->joinScope.close();
            co_await this->contentDeliveryManager->destroy();
            // The info communicator listens on the transport — take it
            // down while the transport is still alive.
            if (this->infoRpcCommunicator) {
                this->infoRpcCommunicator->destroy();
            }
            this->nodeInfoRpcLocal = nullptr;
            this->nodeInfoClient = nullptr;
            this->infoRpcCommunicator = nullptr;
            co_await this->controlLayerNode->stop();
        }
    }
};

} // namespace streamr::trackerlessnetwork
