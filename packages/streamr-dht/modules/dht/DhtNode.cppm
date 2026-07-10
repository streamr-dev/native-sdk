// Module streamr.dht.DhtNode
// Ported from packages/dht/src/dht/DhtNode.ts (v103.8.0-rc.3). The full DHT
// node: it composes PeerManager, PeerDiscovery, Router,
// RecursiveOperationManager and StoreManager over one RoutingRpcCommunicator,
// and implements the Transport interface so a layer-1 DhtNode can use a
// layer-0 DhtNode as its transport.
//
// Scope of this port (phase A8): only the "transport given" path — every test
// constructs a DhtNode over a SimulatorTransport (or, for layer 1, over a
// layer-0 DhtNode). The ConnectionManager/DefaultConnectorFacade path (real
// sockets, connectivity checking, createPeerDescriptor from a connectivity
// response, CDN/GeoIP region) is real-socket territory deferred to milestone B;
// start() throws if no transport is given. The external DhtNode contact events
// (nearbyContactAdded etc.) have no subscriber below milestone C, so Store
// replication and the k-bucket-empty rejoin are wired straight to PeerManager's
// own events instead of being re-emitted; periodic neighbour pinging (an opt-in
// no A8 test enables) is likewise deferred.
module;

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <coroutine> // IWYU pragma: keep

export module streamr.dht.DhtNode;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;
import streamr.utils.ExecutorHelper;
import streamr.utils.waitForCondition;
import streamr.logger.SLogger;
import streamr.dht.ConnectionLocker;
import streamr.dht.ConnectionsView;
import streamr.dht.consts;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtNodeRpcLocal;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.DhtRpcClient;
import streamr.dht.ExternalApiRpcLocal;
import streamr.dht.ExternalApiRpcRemote;
import streamr.dht.Identifiers;
import streamr.dht.LocalDataStore;
import streamr.dht.PeerDiscovery;
import streamr.dht.PeerManager;
import streamr.dht.RecursiveOperationManager;
import streamr.dht.RecursiveOperationSession;
import streamr.dht.ringIdentifiers;
import streamr.dht.Router;
import streamr.dht.RoutingSession;
import streamr.dht.RoutingRpcCommunicator;
import streamr.dht.StoreManager;
import streamr.dht.StoreRpcRemote;
import streamr.dht.Transport;
import streamr.protorpc.RpcCommunicator;

// Hoisted from the former header (file scope, NOT exported).
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;
using streamr::utils::AbortController;
using streamr::utils::waitForCondition;

export namespace streamr::dht {

using ::dht::ClosestPeersRequest;
using ::dht::ClosestPeersResponse;
using ::dht::ClosestRingPeersRequest;
using ::dht::ClosestRingPeersResponse;
using ::dht::DataEntry;
using ::dht::ExternalFetchDataRequest;
using ::dht::ExternalFetchDataResponse;
using ::dht::ExternalStoreDataRequest;
using ::dht::ExternalStoreDataResponse;
using ::dht::LeaveNotice;
using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::PingRequest;
using ::dht::PingResponse;
using ::dht::RecursiveOperation;
using ::dht::RouteMessageWrapper;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::connection::LockID;
using streamr::dht::contact::RingIdRaw;
using streamr::dht::discovery::PeerDiscovery;
using streamr::dht::discovery::PeerDiscoveryOptions;
using streamr::dht::recursiveoperation::RecursiveOperationManager;
using streamr::dht::recursiveoperation::RecursiveOperationManagerOptions;
using streamr::dht::recursiveoperation::RecursiveOperationResult;
using streamr::dht::routing::Router;
using streamr::dht::routing::RouterOptions;
using streamr::dht::routing::RoutingMode;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::store::LocalDataStore;
using streamr::dht::store::StoreManager;
using streamr::dht::store::StoreManagerOptions;
using streamr::dht::store::StoreRpcRemote;
using streamr::dht::transport::RoutingRpcCommunicator;
using streamr::dht::transport::SendOptions;
using streamr::dht::transport::Transport;
using streamr::dht::transport::transportevents::Connected;
using streamr::dht::transport::transportevents::Disconnected;
namespace transportevents = streamr::dht::transport::transportevents;
using streamr::dht::store::StoreRpcClient;
using streamr::protorpc::RpcCommunicatorOptions;
// DhtNodeRpcClient and ExternalApiRpcClient are exported into streamr::dht by
// the imported DhtNodeRpcRemote / ExternalApiRpcRemote modules.

inline constexpr size_t numberOfNodesPerKBucketDefault = 8;

struct DhtNodeOptions {
    ServiceID serviceId = CONTROL_LAYER_NODE_SERVICE_ID;
    size_t joinParallelism = 3;
    size_t maxContactCount = 200;
    size_t numberOfNodesPerKBucket = numberOfNodesPerKBucketDefault;
    size_t joinNoProgressLimit = 5;
    std::chrono::milliseconds dhtJoinTimeout{60000};
    size_t peerDiscoveryQueryBatchSize = 5;
    uint32_t storeHighestTtl = 60000;
    uint32_t storeMaxTtl = 60000;
    std::chrono::milliseconds networkConnectivityTimeout{10000};
    size_t storageRedundancyFactor = 5;
    std::optional<size_t> neighborPingLimit;
    std::optional<std::chrono::milliseconds> rpcRequestTimeout;

    // Given transport (required in this phase). The three roles are the same
    // object for a SimulatorTransport / ConnectionManager.
    Transport* transport = nullptr;
    ConnectionsView* connectionsView = nullptr;
    ConnectionLocker* connectionLocker = nullptr; // may be null
    std::optional<PeerDescriptor> peerDescriptor;
    std::vector<PeerDescriptor> entryPoints;
};

class DhtNode : public Transport {
private:
    static constexpr std::chrono::milliseconds externalApiTimeout{10000};
    static constexpr std::chrono::milliseconds networkConnectivityPollInterval{
        100};

    DhtNodeOptions options;
    LocalDataStore localDataStore;
    std::unique_ptr<RoutingRpcCommunicator> rpcCommunicator;
    Transport* transportPtr = nullptr;
    ConnectionsView* connectionsView = nullptr;
    ConnectionLocker* connectionLocker = nullptr;
    std::shared_ptr<ConnectionLocker>
        connectionLockerShared; // non-owning alias
    std::optional<PeerDescriptor> localPeerDescriptor;
    std::shared_ptr<Router> router;
    std::shared_ptr<StoreManager> storeManager;
    std::shared_ptr<RecursiveOperationManager> recursiveOperationManager;
    std::shared_ptr<PeerDiscovery> peerDiscovery;
    std::shared_ptr<PeerManager> peerManager;
    std::unique_ptr<DhtNodeRpcLocal> dhtNodeRpcLocal;
    std::shared_ptr<ExternalApiRpcLocal> externalApiRpcLocal;
    bool started = false;
    AbortController abortController;
    // Detaches the k-bucket-empty rejoin off the delivery thread that raised
    // the event. Serial view of the shared worker pool (formerly a private
    // single-thread pool — see streamr.utils.SharedExecutors); the scope is
    // drained in stop() so no rejoin outlives this node.
    streamr::utils::SharedSerialExecutor recoveryExecutor{
        streamr::utils::SharedExecutors::worker()};
    streamr::utils::GuardedAsyncScope recoveryScope;
    HandlerToken messageToken;
    HandlerToken connectedToken;
    HandlerToken disconnectedToken;

    [[nodiscard]] std::shared_ptr<DhtNodeRpcRemote> createDhtNodeRpcRemote(
        const PeerDescriptor& peerDescriptor) {
        return std::make_shared<DhtNodeRpcRemote>(
            *this->localPeerDescriptor,
            peerDescriptor,
            this->options.serviceId,
            DhtNodeRpcClient(*this->rpcCommunicator),
            this->options.rpcRequestTimeout);
    }

    // PeerManager::getNeighbors returns the k-bucket remotes; the callers here
    // (and the public getNeighbors) want their descriptors (TS maps them).
    [[nodiscard]] std::vector<PeerDescriptor> getNeighborDescriptors() {
        std::vector<PeerDescriptor> result;
        for (const auto& neighbor : this->peerManager->getNeighbors()) {
            result.push_back(neighbor->getPeerDescriptor());
        }
        return result;
    }

    void handleMessageFromTransport(const Message& message) {
        if (message.serviceid() == this->options.serviceId) {
            this->rpcCommunicator->handleMessageFromPeer(message);
        }
    }

    void handleMessageFromRouter(const Message& message) {
        if (message.serviceid() == this->options.serviceId) {
            this->rpcCommunicator->handleMessageFromPeer(message);
        } else {
            this->emit<transport::transportevents::Message>(message);
        }
    }

    [[nodiscard]] std::vector<PeerDescriptor> getConnectedEntryPoints() {
        std::vector<PeerDescriptor> result;
        for (const auto& entryPoint : this->options.entryPoints) {
            if (this->connectionsView->hasConnection(
                    Identifiers::getNodeIdFromPeerDescriptor(entryPoint))) {
                result.push_back(entryPoint);
            }
        }
        return result;
    }

    template <typename R>
    folly::coro::Task<R> executeRecursiveOperation(
        std::function<folly::coro::Task<R>()> executeDirectly,
        std::function<folly::coro::Task<R>(const PeerDescriptor&)>
            executeViaPeer) {
        const auto connectedEntryPoints = this->getConnectedEntryPoints();
        if (this->peerDiscovery->isJoinOngoing() &&
            !connectedEntryPoints.empty()) {
            // TS uses lodash sample (a random entry point); the first is
            // sufficient and deterministic for the tests.
            co_return co_await executeViaPeer(connectedEntryPoints.front());
        }
        co_return co_await executeDirectly();
    }

    void initPeerManager() {
        this->peerManager = PeerManager::newInstance(
            PeerManagerOptions{
                .numberOfNodesPerKBucket =
                    this->options.numberOfNodesPerKBucket,
                .maxContactCount = this->options.maxContactCount,
                .localNodeId = this->getNodeId(),
                .localPeerDescriptor = *this->localPeerDescriptor,
                .connectionLocker = this->connectionLockerShared,
                .neighborPingLimit = this->options.neighborPingLimit,
                .lockId = LockID{this->options.serviceId},
                .createDhtNodeRpcRemote =
                    [this](const PeerDescriptor& peerDescriptor) {
                        return this->createDhtNodeRpcRemote(peerDescriptor);
                    },
                .hasConnection =
                    [this](const DhtAddress& nodeId) {
                        return this->connectionsView->hasConnection(nodeId);
                    }});
        // StoreManager tracks the neighbourhood via nearbyContactAdded (TS
        // re-emits it as a DhtNode event first; wired straight through here).
        this->peerManager->on<peermanagerevents::NearbyContactAdded>(
            [this](const PeerDescriptor& peerDescriptor) {
                this->storeManager->onContactAdded(peerDescriptor);
            });
        this->peerManager->on<peermanagerevents::KBucketEmpty>([this]() {
            if (!this->peerDiscovery->isJoinOngoing() &&
                !this->options.entryPoints.empty()) {
                // Pin the discovery (shared_ptr) and cancel with the node's
                // abort signal so the detached rejoin is safe across stop().
                auto discovery = this->peerDiscovery;
                const auto token =
                    this->abortController.getSignal().getCancellationToken();
                for (const auto& entryPoint : this->options.entryPoints) {
                    this->recoveryScope.add(
                        streamr::utils::co_withExecutor(
                            &this->recoveryExecutor,
                            streamr::utils::co_withCancellation(
                                token,
                                folly::coro::co_invoke(
                                    [discovery,
                                     entryPoint]() -> folly::coro::Task<void> {
                                        co_await discovery->rejoinDht(
                                            entryPoint);
                                    }))));
                }
            }
        });
        this->connectedToken = this->transportPtr->on<Connected>(
            [this](const PeerDescriptor& peerDescriptor) {
                this->router->onNodeConnected(peerDescriptor);
                this->emit<Connected>(peerDescriptor);
            });
        this->disconnectedToken = this->transportPtr->on<Disconnected>(
            [this](const PeerDescriptor& peerDescriptor, bool gracefulLeave) {
                if (this->connectionLocker != nullptr) {
                    const auto nodeId =
                        Identifiers::getNodeIdFromPeerDescriptor(
                            peerDescriptor);
                    if (gracefulLeave) {
                        this->peerManager->removeContact(nodeId);
                    } else {
                        this->peerManager->removeNeighbor(nodeId);
                    }
                }
                this->router->onNodeDisconnected(peerDescriptor);
                this->emit<Disconnected>(peerDescriptor, gracefulLeave);
            });
    }

    void bindRpcLocalMethods() {
        this->dhtNodeRpcLocal =
            std::make_unique<DhtNodeRpcLocal>(DhtNodeRpcLocalOptions{
                .peerDiscoveryQueryBatchSize =
                    this->options.peerDiscoveryQueryBatchSize,
                .getNeighbors =
                    [this]() { return this->getNeighborDescriptors(); },
                .getClosestRingContactsTo =
                    [this](const RingIdRaw& ringIdRaw, size_t limit) {
                        return this->getClosestRingContactsTo(ringIdRaw, limit);
                    },
                .addContact =
                    [this](const PeerDescriptor& contact) {
                        this->peerManager->addContact(contact);
                    },
                .removeContact =
                    [this](const DhtAddress& nodeId) {
                        this->removeContact(nodeId);
                    }});
        this->rpcCommunicator
            ->registerRpcMethod<ClosestPeersRequest, ClosestPeersResponse>(
                "getClosestPeers",
                [this](
                    const ClosestPeersRequest& req,
                    const DhtCallContext& context) {
                    return this->dhtNodeRpcLocal->getClosestPeers(req, context);
                });
        this->rpcCommunicator->registerRpcMethod<
            ClosestRingPeersRequest,
            ClosestRingPeersResponse>(
            "getClosestRingPeers",
            [this](
                const ClosestRingPeersRequest& req,
                const DhtCallContext& context) {
                return this->dhtNodeRpcLocal->getClosestRingPeers(req, context);
            });
        this->rpcCommunicator->registerRpcMethod<PingRequest, PingResponse>(
            "ping",
            [this](const PingRequest& req, const DhtCallContext& context) {
                return this->dhtNodeRpcLocal->ping(req, context);
            });
        this->rpcCommunicator->registerRpcNotification<LeaveNotice>(
            "leaveNotice",
            [this](const LeaveNotice& req, const DhtCallContext& context) {
                this->dhtNodeRpcLocal->leaveNotice(req, context);
            });
        this->externalApiRpcLocal =
            std::make_shared<ExternalApiRpcLocal>(ExternalApiRpcLocalOptions{
                .executeRecursiveOperation =
                    [this](
                        const DhtAddress& key,
                        RecursiveOperation operation,
                        const DhtAddress& excludedPeer) {
                        return this->recursiveOperationManager->execute(
                            key, operation, excludedPeer);
                    },
                .storeDataToDht =
                    [this](
                        const DhtAddress& key,
                        const ::google::protobuf::Any& data,
                        const DhtAddress& creator) {
                        return this->storeDataToDht(key, data, creator);
                    }});
        auto externalApi = this->externalApiRpcLocal;
        this->rpcCommunicator->registerRpcMethodAsync<
            ExternalFetchDataRequest,
            ExternalFetchDataResponse>(
            "externalFetchData",
            [externalApi](
                const ExternalFetchDataRequest& req,
                const DhtCallContext& context)
                -> folly::coro::Task<ExternalFetchDataResponse> {
                co_return co_await externalApi->externalFetchData(req, context);
            });
        this->rpcCommunicator->registerRpcMethodAsync<
            ExternalStoreDataRequest,
            ExternalStoreDataResponse>(
            "externalStoreData",
            [externalApi](
                const ExternalStoreDataRequest& req,
                const DhtCallContext& context)
                -> folly::coro::Task<ExternalStoreDataResponse> {
                co_return co_await externalApi->externalStoreData(req, context);
            });
    }

public:
    explicit DhtNode(DhtNodeOptions options)
        : options(std::move(options)),
          localDataStore(this->options.storeMaxTtl) {}

    ~DhtNode() override = default;
    DhtNode(const DhtNode&) = delete;
    DhtNode& operator=(const DhtNode&) = delete;
    DhtNode(DhtNode&&) = delete;
    DhtNode& operator=(DhtNode&&) = delete;

    folly::coro::Task<void> start() {
        if (this->started || this->abortController.getSignal().aborted) {
            co_return;
        }
        this->started = true;
        if (this->options.transport == nullptr) {
            throw std::runtime_error(
                "DhtNode requires a transport in this build (the "
                "ConnectionManager path arrives in milestone B)");
        }
        this->transportPtr = this->options.transport;
        this->connectionsView = this->options.connectionsView;
        this->connectionLocker = this->options.connectionLocker;
        if (this->connectionLocker != nullptr) {
            this->connectionLockerShared = std::shared_ptr<ConnectionLocker>(
                this->connectionLocker, [](ConnectionLocker*) {});
        }
        this->localPeerDescriptor =
            this->transportPtr->getLocalPeerDescriptor();

        std::optional<RpcCommunicatorOptions> communicatorOptions;
        if (this->options.rpcRequestTimeout.has_value()) {
            communicatorOptions = RpcCommunicatorOptions{
                .rpcRequestTimeout = *this->options.rpcRequestTimeout};
        }
        this->rpcCommunicator = std::make_unique<RoutingRpcCommunicator>(
            ServiceID{this->options.serviceId},
            [this](Message msg, SendOptions sendOptions) {
                this->transportPtr->send(msg, sendOptions);
            },
            std::move(communicatorOptions));

        this->messageToken = this->transportPtr->on<transportevents::Message>(
            [this](const Message& message) {
                this->handleMessageFromTransport(message);
            });

        this->initPeerManager();

        this->peerDiscovery = PeerDiscovery::newInstance(
            PeerDiscoveryOptions{
                .localPeerDescriptor = *this->localPeerDescriptor,
                .joinNoProgressLimit = this->options.joinNoProgressLimit,
                .serviceId = this->options.serviceId,
                .parallelism = this->options.joinParallelism,
                .joinTimeout = this->options.dhtJoinTimeout,
                .connectionLocker = this->connectionLockerShared,
                .peerManager = *this->peerManager,
                .abortSignal = this->abortController.getSignal(),
                .createDhtNodeRpcRemote =
                    [this](const PeerDescriptor& peerDescriptor) {
                        return this->createDhtNodeRpcRemote(peerDescriptor);
                    }});
        this->router = Router::newInstance(
            RouterOptions{
                .rpcCommunicator = *this->rpcCommunicator,
                .localPeerDescriptor = *this->localPeerDescriptor,
                .handleMessage =
                    [this](const Message& message) {
                        this->handleMessageFromRouter(message);
                    },
                .getConnections =
                    [this]() {
                        return this->connectionsView->getConnections();
                    }});
        auto routerPtr = this->router;
        this->recursiveOperationManager =
            RecursiveOperationManager::newInstance(
                RecursiveOperationManagerOptions{
                    .rpcCommunicator = *this->rpcCommunicator,
                    .sessionTransport = *this,
                    .connectionsView = *this->connectionsView,
                    .localPeerDescriptor = *this->localPeerDescriptor,
                    .serviceId = this->options.serviceId,
                    .localDataStore = this->localDataStore,
                    .addContact =
                        [this](const PeerDescriptor& contact) {
                            this->peerManager->addContact(contact);
                        },
                    .createDhtNodeRpcRemote =
                        [this](const PeerDescriptor& peerDescriptor) {
                            return this->createDhtNodeRpcRemote(peerDescriptor);
                        },
                    .doRouteMessage =
                        [routerPtr](
                            const RouteMessageWrapper& message,
                            RoutingMode mode,
                            const std::optional<DhtAddress>& excludedPeer) {
                            return routerPtr->doRouteMessage(
                                message, mode, excludedPeer);
                        },
                    .isMostLikelyDuplicate =
                        [routerPtr](const std::string& requestId) {
                            return routerPtr->isMostLikelyDuplicate(requestId);
                        },
                    .addToDuplicateDetector =
                        [routerPtr](const std::string& requestId) {
                            routerPtr->addToDuplicateDetector(requestId);
                        }});
        this->storeManager = StoreManager::newInstance(
            StoreManagerOptions{
                .rpcCommunicator = *this->rpcCommunicator,
                .localPeerDescriptor = *this->localPeerDescriptor,
                .localDataStore = this->localDataStore,
                .serviceId = this->options.serviceId,
                .highestTtl = this->options.storeHighestTtl,
                .redundancyFactor = this->options.storageRedundancyFactor,
                .getNeighbors =
                    [this]() { return this->getNeighborDescriptors(); },
                .createRpcRemote =
                    [this](const PeerDescriptor& contact) {
                        return std::make_shared<StoreRpcRemote>(
                            *this->localPeerDescriptor,
                            contact,
                            StoreRpcClient(*this->rpcCommunicator),
                            this->options.rpcRequestTimeout);
                    },
                .executeRecursiveOperation =
                    [this](
                        const DhtAddress& key, RecursiveOperation operation) {
                        return this->recursiveOperationManager->execute(
                            key, operation);
                    }});
        this->bindRpcLocalMethods();
        co_return;
    }

    // --- Transport interface ---------------------------------------------

    void send(
        const Message& message, const SendOptions& /*sendOptions*/) override {
        if (!this->started || this->abortController.getSignal().aborted) {
            return;
        }
        const auto reachableThrough = this->peerDiscovery->isJoinOngoing()
            ? this->getConnectedEntryPoints()
            : std::vector<PeerDescriptor>{};
        this->router->send(message, reachableThrough);
    }

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const override {
        return *this->localPeerDescriptor;
    }

    void stop() override {
        if (this->abortController.getSignal().aborted || !this->started) {
            return;
        }
        SLogger::trace("DhtNode::stop()");
        this->abortController.abort();
        // Drain detached rejoins before tearing the components down (the
        // abort above cancels them; this replaces the former private
        // recovery executor's destructor join). A KBucketEmpty still firing
        // mid-join is dropped by the scope's close() gate.
        this->recoveryScope.close();
        this->transportPtr->off<transportevents::Message>(this->messageToken);
        this->transportPtr->off<Connected>(this->connectedToken);
        this->transportPtr->off<Disconnected>(this->disconnectedToken);
        streamr::utils::blockingWait(this->storeManager->destroy());
        this->localDataStore.clear();
        if (this->peerManager) {
            this->peerManager->stop();
        }
        this->router->stop();
        this->recursiveOperationManager->stop();
        // The transport was given, not created here, so it is not stopped.
        this->connectionLocker = nullptr;
    }

    // --- Public API ------------------------------------------------------

    folly::coro::Task<void> joinDht(
        std::vector<PeerDescriptor> entryPointDescriptors,
        bool doAdditionalDistantPeerDiscovery = true,
        bool retry = true) {
        if (!this->started) {
            throw std::runtime_error(
                "Cannot join DHT before calling start() on DhtNode");
        }
        co_await this->peerDiscovery->joinDht(
            std::move(entryPointDescriptors),
            doAdditionalDistantPeerDiscovery,
            retry);
    }

    folly::coro::Task<void> joinRing() {
        if (!this->started) {
            throw std::runtime_error(
                "Cannot join ring before calling start() on DhtNode");
        }
        co_await this->peerDiscovery->joinRing();
    }

    folly::coro::Task<std::vector<PeerDescriptor>> storeDataToDht(
        const DhtAddress& key,
        const ::google::protobuf::Any& data,
        std::optional<DhtAddress> creator = std::nullopt) {
        const DhtAddress effectiveCreator = creator.value_or(this->getNodeId());
        co_return co_await this
            ->executeRecursiveOperation<std::vector<PeerDescriptor>>(
                [this, key, data, effectiveCreator]()
                    -> folly::coro::Task<std::vector<PeerDescriptor>> {
                    co_return co_await this->storeManager->storeDataToDht(
                        key, data, effectiveCreator);
                },
                [this, key, data](const PeerDescriptor& connectedEntryPoint)
                    -> folly::coro::Task<std::vector<PeerDescriptor>> {
                    co_return co_await this->storeDataToDhtViaPeer(
                        key, data, connectedEntryPoint);
                });
    }

    folly::coro::Task<std::vector<PeerDescriptor>> storeDataToDhtViaPeer(
        const DhtAddress& key,
        const ::google::protobuf::Any& data,
        PeerDescriptor peer) {
        ExternalApiRpcRemote rpcRemote(
            *this->localPeerDescriptor,
            std::move(peer),
            ExternalApiRpcClient(*this->rpcCommunicator));
        co_return co_await rpcRemote.storeData(key, data);
    }

    folly::coro::Task<std::vector<DataEntry>> fetchDataFromDht(
        const DhtAddress& key) {
        co_return co_await this
            ->executeRecursiveOperation<std::vector<DataEntry>>(
                [this, key]() -> folly::coro::Task<std::vector<DataEntry>> {
                    const auto result =
                        co_await this->recursiveOperationManager->execute(
                            key, RecursiveOperation::FETCH_DATA);
                    co_return result.dataEntries;
                },
                [this, key](const PeerDescriptor& connectedEntryPoint)
                    -> folly::coro::Task<std::vector<DataEntry>> {
                    co_return co_await this->fetchDataFromDhtViaPeer(
                        key, connectedEntryPoint);
                });
    }

    folly::coro::Task<std::vector<DataEntry>> fetchDataFromDhtViaPeer(
        const DhtAddress& key, PeerDescriptor peer) {
        ExternalApiRpcRemote rpcRemote(
            *this->localPeerDescriptor,
            std::move(peer),
            ExternalApiRpcClient(*this->rpcCommunicator));
        co_return co_await rpcRemote.externalFetchData(key);
    }

    folly::coro::Task<std::vector<PeerDescriptor>> findClosestNodesFromDht(
        const DhtAddress& key) {
        co_return co_await this
            ->executeRecursiveOperation<std::vector<PeerDescriptor>>(
                [this,
                 key]() -> folly::coro::Task<std::vector<PeerDescriptor>> {
                    const auto result =
                        co_await this->recursiveOperationManager->execute(
                            key, RecursiveOperation::FIND_CLOSEST_NODES);
                    co_return result.closestNodes;
                },
                [this, key](const PeerDescriptor& connectedEntryPoint)
                    -> folly::coro::Task<std::vector<PeerDescriptor>> {
                    ExternalApiRpcRemote rpcRemote(
                        *this->localPeerDescriptor,
                        connectedEntryPoint,
                        ExternalApiRpcClient(*this->rpcCommunicator));
                    co_return co_await rpcRemote.externalFindClosestNode(key);
                });
    }

    folly::coro::Task<void> waitForNetworkConnectivity() {
        co_await waitForCondition(
            [this]() {
                return this->connectionsView->getConnectionCount() > 0;
            },
            this->options.networkConnectivityTimeout,
            networkConnectivityPollInterval,
            &this->abortController.getSignal());
    }

    [[nodiscard]] DhtAddress getNodeId() {
        return Identifiers::getNodeIdFromPeerDescriptor(
            *this->localPeerDescriptor);
    }

    [[nodiscard]] size_t getNeighborCount() {
        return this->peerManager->getNeighborCount();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getNeighbors() {
        if (!this->started) {
            return {};
        }
        return this->getNeighborDescriptors();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getClosestContacts(
        std::optional<size_t> limit = std::nullopt) {
        return this->peerManager->getClosestContacts(limit);
    }

    [[nodiscard]] std::vector<PeerDescriptor> getRandomContacts(
        std::optional<size_t> limit = std::nullopt) {
        return this->peerManager->getRandomContacts(limit);
    }

    [[nodiscard]] ClosestRingPeerDescriptors getRingContacts() {
        return this->peerManager->getRingContacts();
    }

    [[nodiscard]] ClosestRingPeerDescriptors getClosestRingContactsTo(
        const RingIdRaw& ringIdRaw,
        std::optional<size_t> limit = std::nullopt) {
        const auto closest =
            this->peerManager->getClosestRingContactsTo(ringIdRaw, limit);
        ClosestRingPeerDescriptors result;
        for (const auto& contact : closest.left) {
            result.left.push_back(contact->getPeerDescriptor());
        }
        for (const auto& contact : closest.right) {
            result.right.push_back(contact->getPeerDescriptor());
        }
        return result;
    }

    void removeContact(const DhtAddress& nodeId) {
        if (!this->started) {
            return;
        }
        this->peerManager->removeContact(nodeId);
    }

    [[nodiscard]] bool hasJoined() {
        return this->peerDiscovery->isJoinCalled();
    }

    [[nodiscard]] ConnectionsView* getConnectionsView() {
        return this->connectionsView;
    }
};

} // namespace streamr::dht
