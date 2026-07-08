// Module streamr.dht.StoreManager
// Ported from packages/dht/src/dht/store/StoreManager.ts (v103.8.0-rc.3).
// Owns the store side of a DHT node: registers the StoreRpc handlers, stores
// data to the k closest nodes (storeDataToDht), and keeps replicas in sync
// as the neighbourhood changes (onContactAdded / replicateDataToClosestNodes).
//
// C++ notes: the recursive-find and per-contact remote creation are options
// callbacks (matching this package's callback-options style, and letting the
// unit test mock them); the fire-and-forget replications TS runs via
// setImmediate are dispatched onto a small worker executor here, so
// StoreManager is owned through a shared_ptr.
module;
#include <new>



export module streamr.dht.StoreManager;

import std;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.ExecutorHelper;
import streamr.logger.SLogger;
import streamr.protorpc.RpcCommunicator;
import streamr.dht.DhtCallContext;
import streamr.dht.getClosestNodes;
import streamr.dht.Identifiers;
import streamr.dht.LocalDataStore;
import streamr.dht.RecursiveOperationSession;
import streamr.dht.StoreRpcLocal;
import streamr.dht.StoreRpcRemote;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;

export namespace streamr::dht::store {

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperation;
using ::dht::ReplicateDataRequest;
using ::dht::StoreDataRequest;
using ::dht::StoreDataResponse;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::contact::GetClosestNodesOptions;
using streamr::dht::recursiveoperation::RecursiveOperationResult;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::protorpc::RpcCommunicator;

struct StoreManagerOptions {
    RpcCommunicator<DhtCallContext>& rpcCommunicator;
    PeerDescriptor localPeerDescriptor;
    LocalDataStore& localDataStore;
    ServiceID serviceId;
    std::uint32_t highestTtl;
    std::size_t redundancyFactor;
    std::function<std::vector<PeerDescriptor>()> getNeighbors;
    std::function<std::shared_ptr<StoreRpcRemote>(const PeerDescriptor&)>
        createRpcRemote;
    // recursiveOperationManager.execute(key, FIND_CLOSEST_NODES)
    std::function<folly::coro::Task<RecursiveOperationResult>(
        const DhtAddress&, RecursiveOperation)>
        executeRecursiveOperation;
};

class StoreManager : public EnableSharedFromThis {
private:
    static constexpr std::size_t replicationWorkerThreads = 1;

    StoreManagerOptions options;
    folly::CPUThreadPoolExecutor replicationExecutor{replicationWorkerThreads};
    std::unique_ptr<StoreRpcLocal> rpcLocal;

    explicit StoreManager(StoreManagerOptions options)
        : options(std::move(options)) {}

    [[nodiscard]] bool equalsLocal(const PeerDescriptor& peer) const {
        return Identifiers::areEqualPeerDescriptors(
            peer, this->options.localPeerDescriptor);
    }

    // The actual async replication of one entry to one contact.
    folly::coro::Task<void> doReplicate(
        DataEntry dataEntry, PeerDescriptor contact) {
        auto rpcRemote = this->options.createRpcRemote(contact);
        ReplicateDataRequest request;
        *request.mutable_entry() = std::move(dataEntry);
        try {
            co_await rpcRemote->replicateData(std::move(request), true);
        } catch (const std::exception& err) {
            SLogger::trace(
                "replicateData() threw an exception " +
                std::string(err.what()));
        }
    }

    // Fire-and-forget the async replication onto the worker executor.
    void replicateDataToContact(
        const DataEntry& dataEntry, const PeerDescriptor& contact) {
        auto self = this->sharedFromThis<StoreManager>();
        DataEntry entry = dataEntry;
        PeerDescriptor target = contact;
        streamr::utils::co_withExecutor(
            &this->replicationExecutor,
            folly::coro::co_invoke(
                [self, entry, target]() -> folly::coro::Task<void> {
                    co_await self->doReplicate(entry, target);
                }))
            .start();
    }

    void registerLocalRpcMethods() {
        auto self = this->sharedFromThis<StoreManager>();
        this->rpcLocal = std::make_unique<StoreRpcLocal>(StoreRpcLocalOptions{
            .localDataStore = this->options.localDataStore,
            .localPeerDescriptor = this->options.localPeerDescriptor,
            .replicateDataToContact =
                [this](
                    const DataEntry& dataEntry, const PeerDescriptor& contact) {
                    this->replicateDataToContact(dataEntry, contact);
                },
            .getStorers =
                [this](const DhtAddress& dataKey) {
                    return this->getStorers(dataKey, std::nullopt);
                }});
        std::weak_ptr<StoreManager> weakSelf = self;
        this->options.rpcCommunicator
            .template registerRpcMethod<StoreDataRequest, StoreDataResponse>(
                "storeData",
                [weakSelf](
                    const StoreDataRequest& request,
                    const DhtCallContext& callContext) -> StoreDataResponse {
                    auto locked = weakSelf.lock();
                    if (!locked) {
                        return StoreDataResponse{};
                    }
                    return locked->rpcLocal->storeData(request, callContext);
                });
        this->options.rpcCommunicator
            .template registerRpcNotification<ReplicateDataRequest>(
                "replicateData",
                [weakSelf](
                    const ReplicateDataRequest& request,
                    const DhtCallContext& callContext) {
                    auto locked = weakSelf.lock();
                    if (locked) {
                        locked->rpcLocal->replicateData(request, callContext);
                    }
                });
    }

    void replicateAndUpdateStaleState(
        const DhtAddress& dataKey, const PeerDescriptor& newNode) {
        const auto storers = this->getStorers(dataKey, std::nullopt);
        std::vector<PeerDescriptor> storersBeforeContactAdded;
        for (const auto& peer : storers) {
            if (!Identifiers::areEqualPeerDescriptors(peer, newNode)) {
                storersBeforeContactAdded.push_back(peer);
            }
        }
        const bool selfWasPrimaryStorer = !storersBeforeContactAdded.empty() &&
            this->equalsLocal(storersBeforeContactAdded[0]);
        const bool newNodeIsStorer = std::ranges::any_of(
            storers, [&newNode](const PeerDescriptor& peer) {
                return Identifiers::areEqualPeerDescriptors(peer, newNode);
            });
        if (selfWasPrimaryStorer) {
            if (newNodeIsStorer) {
                auto self = this->sharedFromThis<StoreManager>();
                DhtAddress key = dataKey;
                PeerDescriptor node = newNode;
                streamr::utils::co_withExecutor(
                    &this->replicationExecutor,
                    folly::coro::co_invoke(
                        [self, key, node]() -> folly::coro::Task<void> {
                            const auto dataEntries =
                                self->options.localDataStore.values(key);
                            for (const auto& dataEntry : dataEntries) {
                                co_await self->doReplicate(dataEntry, node);
                            }
                        }))
                    .start();
            }
        } else if (!std::ranges::any_of(
                       storers, [this](const PeerDescriptor& peer) {
                           return this->equalsLocal(peer);
                       })) {
            this->options.localDataStore.setAllEntriesAsStale(dataKey);
        }
    }

    [[nodiscard]] std::vector<PeerDescriptor> getStorers(
        const DhtAddress& dataKey, std::optional<PeerDescriptor> excludedNode) {
        std::vector<PeerDescriptor> nodes = this->options.getNeighbors();
        nodes.push_back(this->options.localPeerDescriptor);
        GetClosestNodesOptions opts;
        opts.maxCount = this->options.redundancyFactor;
        if (excludedNode.has_value()) {
            opts.excludedNodeIds = std::set<DhtAddress>{
                Identifiers::getNodeIdFromPeerDescriptor(excludedNode.value())};
        }
        return getClosestNodes(dataKey, nodes, opts);
    }

    folly::coro::Task<void> replicateDataToClosestNodes() {
        const auto dataEntries = this->options.localDataStore.values();
        for (const auto& dataEntry : dataEntries) {
            const DhtAddress dataKey = Identifiers::getDhtAddressFromRaw(
                DhtAddressRaw{dataEntry.key()});
            const auto neighbors = getClosestNodes(
                dataKey,
                this->options.getNeighbors(),
                GetClosestNodesOptions{
                    .maxCount = this->options.redundancyFactor});
            for (const auto& neighbor : neighbors) {
                auto rpcRemote = this->options.createRpcRemote(neighbor);
                ReplicateDataRequest request;
                *request.mutable_entry() = dataEntry;
                try {
                    co_await rpcRemote->replicateData(
                        std::move(request), false);
                } catch (const std::exception& err) {
                    SLogger::trace(
                        "Failed to replicate in replicateDataToClosestNodes " +
                        std::string(err.what()));
                }
            }
        }
    }

public:
    [[nodiscard]] static std::shared_ptr<StoreManager> newInstance(
        StoreManagerOptions options) {
        struct MakeSharedEnabler : public StoreManager {
            explicit MakeSharedEnabler(StoreManagerOptions options)
                : StoreManager(std::move(options)) {}
        };
        auto instance = std::make_shared<MakeSharedEnabler>(std::move(options));
        instance->registerLocalRpcMethods();
        return instance;
    }

    ~StoreManager() override = default;

    void onContactAdded(const PeerDescriptor& peerDescriptor) {
        for (const auto& key : this->options.localDataStore.keys()) {
            this->replicateAndUpdateStaleState(key, peerDescriptor);
        }
    }

    folly::coro::Task<std::vector<PeerDescriptor>> storeDataToDht(
        DhtAddress key, ::google::protobuf::Any data, DhtAddress creator) {
        auto self = this->sharedFromThis<StoreManager>();
        SLogger::debug("Storing data to DHT");
        const auto result = co_await this->options.executeRecursiveOperation(
            key, RecursiveOperation::FIND_CLOSEST_NODES);
        const auto& closestNodes = result.closestNodes;
        std::vector<PeerDescriptor> successfulNodes;
        const std::uint32_t ttl = this->options.highestTtl;
        const auto createdAt = nowTimestamp();
        const DhtAddressRaw keyRaw = Identifiers::getRawFromDhtAddress(key);
        const DhtAddressRaw creatorRaw =
            Identifiers::getRawFromDhtAddress(creator);
        for (std::size_t i = 0; i < closestNodes.size() &&
             successfulNodes.size() < this->options.redundancyFactor;
             ++i) {
            if (this->equalsLocal(closestNodes[i])) {
                DataEntry entry;
                entry.set_key(keyRaw);
                *entry.mutable_data() = data;
                entry.set_creator(creatorRaw);
                *entry.mutable_createdat() = createdAt;
                *entry.mutable_storedat() = nowTimestamp();
                entry.set_ttl(ttl);
                entry.set_stale(false);
                entry.set_deleted(false);
                this->options.localDataStore.storeEntry(entry);
                successfulNodes.push_back(closestNodes[i]);
                continue;
            }
            auto rpcRemote = this->options.createRpcRemote(closestNodes[i]);
            StoreDataRequest request;
            request.set_key(keyRaw);
            *request.mutable_data() = data;
            request.set_creator(creatorRaw);
            *request.mutable_createdat() = createdAt;
            request.set_ttl(ttl);
            try {
                co_await rpcRemote->storeData(std::move(request));
                successfulNodes.push_back(closestNodes[i]);
            } catch (const std::exception& err) {
                SLogger::trace(
                    "remote.storeData() threw an exception " +
                    std::string(err.what()));
            }
        }
        co_return successfulNodes;
    }

    folly::coro::Task<void> destroy() {
        co_await this->replicateDataToClosestNodes();
    }
};

} // namespace streamr::dht::store
