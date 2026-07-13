// Module streamr.dht.RecursiveOperationManager
// Ported from packages/dht/src/dht/recursive-operation/
// RecursiveOperationManager.ts (v103.8.0-rc.3). Drives recursive DHT
// operations (find-closest-nodes, fetch-data, delete-data): it forwards a
// routeRequest towards the target over the A4 Router (RECURSIVE mode),
// answers with the closest connected nodes and any locally stored data,
// and runs an initiator-side RecursiveOperationSession to collect results.
//
// The three Router operations are taken as callbacks (doRouteMessage /
// isMostLikelyDuplicate / addToDuplicateDetector) rather than a Router
// handle, matching this package's callback-options style (PeerManager,
// DhtNodeRpcLocal, RouterRpcLocal) and letting the unit test substitute a
// mock router.
module;

#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <cstddef>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module streamr.dht.RecursiveOperationManager;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;
import streamr.utils.waitForEvent;
import streamr.logger.SLogger;
import streamr.protorpc.RpcCommunicator;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.DhtRpcClient;
import streamr.dht.getPeerDistance;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.LocalDataStore;
import streamr.dht.RecursiveOperationRpcLocal;
import streamr.dht.RecursiveOperationSession;
import streamr.dht.RecursiveOperationSessionRpcRemote;
import streamr.dht.RouterRpcLocal;
import streamr.dht.RoutingSession;
import streamr.dht.SortedContactList;
import streamr.dht.Transport;
import streamr.dht.ConnectionsView;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;

export namespace streamr::dht::recursiveoperation {

using ::dht::DataEntry;
using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperation;
using ::dht::RecursiveOperationRequest;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageError;
using ::dht::RouteMessageWrapper;
using streamr::dht::DhtAddress;
using streamr::dht::DhtNodeRpcRemote;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::contact::SortedContactList;
using streamr::dht::contact::SortedContactListOptions;
using streamr::dht::helpers::getPeerDistance;
using streamr::dht::routing::createRouteMessageAck;
using streamr::dht::routing::RoutingMode;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::store::LocalDataStore;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::RpcCommunicatorOptions;

using RecursiveOperationSessionRpcClient =
    ::dht::RecursiveOperationSessionRpcClient<DhtCallContext>;

struct RecursiveOperationManagerOptions {
    RpcCommunicator<DhtCallContext>& rpcCommunicator;
    Transport& sessionTransport;
    ConnectionsView& connectionsView;
    PeerDescriptor localPeerDescriptor;
    ServiceID serviceId;
    LocalDataStore& localDataStore;
    std::function<void(const PeerDescriptor&)> addContact;
    std::function<std::shared_ptr<DhtNodeRpcRemote>(const PeerDescriptor&)>
        createDhtNodeRpcRemote;
    // The Router operations (see the module comment).
    std::function<RouteMessageAck(
        const RouteMessageWrapper&, RoutingMode, std::optional<DhtAddress>)>
        doRouteMessage;
    std::function<bool(const std::string&)> isMostLikelyDuplicate;
    std::function<void(const std::string&)> addToDuplicateDetector;
};

class RecursiveOperationManager : public EnableSharedFromThis {
private:
    static constexpr size_t closestConnectedNodesCount = 5;
    static constexpr std::chrono::milliseconds sessionRpcTimeout{10000};
    static constexpr std::chrono::milliseconds deleteWaitTime{50};

    RecursiveOperationManagerOptions options;
    std::recursive_mutex mutex;
    std::map<std::string, std::shared_ptr<RecursiveOperationSession>>
        ongoingSessions;
    // Completed sessions are RETIRED here instead of dying at the end of
    // execute(): a routed message for the session may still be mid-flight
    // through the session communicator's transport handler on another
    // thread (or the execute() continuation may run inline inside that
    // very handler), and destroying the session there is a
    // use-after-free. The bounded ring defers destruction by
    // retiredSessionCapacity completed sessions — long past any handler
    // invocation. Guarded by this->mutex.
    static constexpr size_t retiredSessionCapacity = 16;
    std::deque<std::shared_ptr<RecursiveOperationSession>> retiredSessions;
    bool stopped = false;
    std::unique_ptr<RecursiveOperationRpcLocal> rpcLocal;
    // sendResponse()'s per-session communicators whose internal scopes
    // were still non-empty when their detached send task finished with
    // them. Destroying a communicator joins its async scope; doing that on
    // a shared worker-pool thread while the scope still holds a task parks
    // the pool thread waiting for work that itself needs a pool thread —
    // with every worker parked in such a join the pool deadlocks
    // permanently (linux-arm64 CI: the former in-handler destruction hung
    // DhtNodeExternalApiTest.ExternalStoreDataHappyPath for its 1200 s
    // timeout; reproduced deterministically with a 1-thread pool). Retired
    // communicators are destroyed only once their scopes are empty (an
    // empty-scope join needs no pool thread) or in stop(), which runs on
    // the owner's thread, never on the pool.
    std::vector<std::shared_ptr<ListeningRpcCommunicator>>
        retiredSessionCommunicators;
    // Runs sendResponse()'s detached fire-and-forget send tasks. Declared
    // AFTER retiredSessionCommunicators so that its destructor (which
    // joins the tasks) runs BEFORE the list dies: a draining task may
    // still retire its communicator into the list.
    streamr::utils::GuardedAsyncScope sendResponseScope;

    // Called under this->mutex. Frees every retired communicator whose
    // send task has settled; the rest stay retired until the next call or
    // stop(). Growth is bounded: each pending entry settles within
    // sessionRpcTimeout.
    void pruneRetiredSessionCommunicators() {
        std::erase_if(
            this->retiredSessionCommunicators,
            [](const std::shared_ptr<ListeningRpcCommunicator>& communicator) {
                return communicator->pendingAsyncTaskCount() == 0;
            });
    }

    explicit RecursiveOperationManager(RecursiveOperationManagerOptions options)
        : options(std::move(options)) {}

    void registerLocalRpcMethods() {
        this->rpcLocal = std::make_unique<RecursiveOperationRpcLocal>(
            RecursiveOperationRpcLocalOptions{
                .doRouteRequest =
                    [this](const RouteMessageWrapper& routedMessage) {
                        return this->doRouteRequest(
                            routedMessage, std::nullopt);
                    },
                .addContact =
                    [this](const PeerDescriptor& contact, bool /*setActive*/) {
                        this->options.addContact(contact);
                    },
                .isMostLikelyDuplicate =
                    [this](const std::string& requestId) {
                        return this->options.isMostLikelyDuplicate(requestId);
                    },
                .addToDuplicateDetector =
                    [this](const std::string& requestId) {
                        this->options.addToDuplicateDetector(requestId);
                    }});
        std::weak_ptr<RecursiveOperationManager> weakSelf =
            this->sharedFromThis<RecursiveOperationManager>();
        this->options.rpcCommunicator
            .template registerRpcMethod<RouteMessageWrapper, RouteMessageAck>(
                "routeRequest",
                [weakSelf](
                    const RouteMessageWrapper& routedMessage,
                    const DhtCallContext& callContext) -> RouteMessageAck {
                    auto self = weakSelf.lock();
                    if (!self || self->stopped) {
                        return createRouteMessageAck(
                            routedMessage, RouteMessageError::STOPPED);
                    }
                    return self->rpcLocal->routeRequest(
                        routedMessage, callContext);
                });
    }

    [[nodiscard]] std::vector<PeerDescriptor> getClosestConnectedNodes(
        const DhtAddress& referenceId, size_t limit) {
        std::vector<std::shared_ptr<DhtNodeRpcRemote>> connectedNodes;
        for (const auto& connection :
             this->options.connectionsView.getConnections()) {
            connectedNodes.push_back(
                this->options.createDhtNodeRpcRemote(connection));
        }
        SortedContactList<DhtNodeRpcRemote> sorted(
            SortedContactListOptions{
                .referenceId = referenceId,
                .allowToContainReferenceId = true,
                .maxSize = limit});
        sorted.addContacts(connectedNodes);
        std::vector<PeerDescriptor> result;
        for (const auto& peer :
             sorted.getClosestContacts(static_cast<int>(limit))) {
            result.push_back(peer->getPeerDescriptor());
        }
        return result;
    }

    [[nodiscard]] bool isPeerCloserToIdThanSelf(
        const PeerDescriptor& peer, const DhtAddress& nodeIdOrDataKey) {
        const DhtAddressRaw raw =
            Identifiers::getRawFromDhtAddress(nodeIdOrDataKey);
        const double distance1 =
            getPeerDistance(DhtAddressRaw{peer.nodeid()}, raw);
        const double distance2 = getPeerDistance(
            DhtAddressRaw{this->options.localPeerDescriptor.nodeid()}, raw);
        return distance1 < distance2;
    }

    void sendResponse(
        const std::vector<PeerDescriptor>& routingPath,
        const PeerDescriptor& targetPeerDescriptor,
        const ServiceID& serviceId,
        const std::vector<PeerDescriptor>& closestConnectedNodes,
        const std::vector<DataEntry>& dataEntries,
        bool noCloserNodesFound) {
        const bool isOwnNode = Identifiers::areEqualPeerDescriptors(
            this->options.localPeerDescriptor, targetPeerDescriptor);
        const auto it = this->ongoingSessions.find(serviceId);
        if (isOwnNode && it != this->ongoingSessions.end()) {
            it->second->onResponseReceived(
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->options.localPeerDescriptor),
                routingPath,
                closestConnectedNodes,
                dataEntries,
                noCloserNodesFound);
        } else {
            // Fire-and-forget response send. This method runs on shared
            // worker-pool threads (RPC server handlers via doRouteRequest),
            // so nothing here may park the thread: neither a blockingWait
            // on the notify (a park bounded by sessionRpcTimeout, but a
            // real thread lost for its duration under pool saturation) nor
            // the per-session communicator's destructor (whose scope join
            // once parked every worker permanently, see
            // retiredSessionCommunicators). The whole send therefore runs
            // as a detached task on sendResponseScope: it co_awaits the
            // notify (suspends instead of parking) and then disposes of
            // the communicator without ever joining a non-empty scope from
            // a pool thread.
            //
            // Everything, including the communicator, is constructed
            // INSIDE the task: if the scope has already been closed by
            // stop(), the dropped lambda destroys only plain values
            // (vectors and protobuf messages) here at the add() call site
            // — dropping a response after stop is correct for a
            // fire-and-forget send, and the drop cannot block.
            std::weak_ptr<RecursiveOperationManager> weakSelf =
                this->sharedFromThis<RecursiveOperationManager>();
            this->sendResponseScope.add(
                streamr::utils::co_withExecutor(
                    &streamr::utils::SharedExecutors::worker(),
                    folly::coro::co_invoke(
                        [weakSelf,
                         serviceId,
                         targetPeerDescriptor,
                         routingPath,
                         closestConnectedNodes,
                         dataEntries,
                         noCloserNodesFound]() mutable
                            -> folly::coro::Task<void> {
                            // The manager outlives every task here: its
                            // owner keeps a reference across stop(), whose
                            // close() drains this scope. The lock can only
                            // fail if the manager is torn down without
                            // stop() (the scope member's destructor join),
                            // in which case nothing has been created yet
                            // and dropping the response is correct.
                            auto self = weakSelf.lock();
                            if (!self) {
                                co_return;
                            }
                            std::shared_ptr<ListeningRpcCommunicator>
                                remoteCommunicator;
                            try {
                                remoteCommunicator =
                                    std::make_shared<ListeningRpcCommunicator>(
                                        ServiceID{serviceId},
                                        self->options.sessionTransport,
                                        RpcCommunicatorOptions{
                                            .rpcRequestTimeout =
                                                sessionRpcTimeout});
                                RecursiveOperationSessionRpcRemote rpcRemote(
                                    self->options.localPeerDescriptor,
                                    targetPeerDescriptor,
                                    RecursiveOperationSessionRpcClient(
                                        *remoteCommunicator),
                                    sessionRpcTimeout);
                                co_await rpcRemote.sendResponse(
                                    std::move(routingPath),
                                    std::move(closestConnectedNodes),
                                    std::move(dataEntries),
                                    noCloserNodesFound);
                            } catch (...) {
                                // The scope requires tasks that never
                                // complete with an exception; a lost
                                // response is fine (fire-and-forget).
                                SLogger::debug(
                                    "sendResponse task failed to send "
                                    "RecursiveOperationResponse");
                            }
                            if (remoteCommunicator) {
                                remoteCommunicator->destroy();
                                // Dispose of the communicator. The notify
                                // has completed, but the send task it
                                // queued on the communicator's own scope
                                // may still have a tiny unfinished tail —
                                // and this coroutine may even have been
                                // resumed from inside that very task, so
                                // joining a non-empty scope here could
                                // self-deadlock. A zero count proves both
                                // that the join is a no-op and that we are
                                // not running inside one of its tasks, so
                                // the communicator may die on this thread;
                                // destroy() above detached the transport
                                // listeners, so the count can no longer
                                // grow and the zero observation is stable.
                                // Otherwise retire it; the pruning in
                                // later sendResponse calls and stop() is
                                // the backstop.
                                if (remoteCommunicator
                                        ->pendingAsyncTaskCount() != 0) {
                                    std::scoped_lock lock(self->mutex);
                                    self->pruneRetiredSessionCommunicators();
                                    self->retiredSessionCommunicators.push_back(
                                        std::move(remoteCommunicator));
                                }
                            }
                        })));
        }
    }

    RouteMessageAck doRouteRequest(
        const RouteMessageWrapper& routedMessage,
        std::optional<DhtAddress> excludedPeer) {
        std::scoped_lock lock(this->mutex);
        if (this->stopped) {
            return createRouteMessageAck(
                routedMessage, RouteMessageError::STOPPED);
        }
        if (!routedMessage.message().has_recursiveoperationrequest()) {
            throw std::runtime_error(
                "routeRequest payload is not a RecursiveOperationRequest");
        }
        const DhtAddress targetId = Identifiers::getDhtAddressFromRaw(
            DhtAddressRaw{routedMessage.target()});
        const RecursiveOperationRequest& request =
            routedMessage.message().recursiveoperationrequest();
        const auto closestConnectedNodes = this->getClosestConnectedNodes(
            targetId, closestConnectedNodesCount);
        std::vector<DataEntry> dataEntries;
        if (request.operation() == RecursiveOperation::FETCH_DATA) {
            dataEntries = this->options.localDataStore.values(targetId);
        }
        if (request.operation() == RecursiveOperation::DELETE_DATA) {
            this->options.localDataStore.markAsDeleted(
                targetId,
                Identifiers::getNodeIdFromPeerDescriptor(
                    routedMessage.sourcepeer()));
        }
        const std::vector<PeerDescriptor> routingPath(
            routedMessage.routingpath().begin(),
            routedMessage.routingpath().end());
        if (this->options.localPeerDescriptor.nodeid() ==
            routedMessage.target()) {
            this->sendResponse(
                routingPath,
                routedMessage.sourcepeer(),
                ServiceID{request.sessionid()},
                closestConnectedNodes,
                dataEntries,
                true);
            return createRouteMessageAck(routedMessage);
        }
        const RouteMessageAck ack = this->options.doRouteMessage(
            routedMessage, RoutingMode::RECURSIVE, excludedPeer);
        if (!ack.has_error() || ack.error() == RouteMessageError::NO_TARGETS) {
            const bool noCloserContactsFound =
                (ack.has_error() &&
                 ack.error() == RouteMessageError::NO_TARGETS) ||
                (!closestConnectedNodes.empty() &&
                 getPreviousPeer(routedMessage).has_value() &&
                 !this->isPeerCloserToIdThanSelf(
                     closestConnectedNodes[0], targetId));
            this->sendResponse(
                routingPath,
                routedMessage.sourcepeer(),
                ServiceID{request.sessionid()},
                closestConnectedNodes,
                dataEntries,
                noCloserContactsFound);
        }
        return ack;
    }

public:
    [[nodiscard]] static std::shared_ptr<RecursiveOperationManager> newInstance(
        RecursiveOperationManagerOptions options) {
        struct MakeSharedEnabler : public RecursiveOperationManager {
            explicit MakeSharedEnabler(RecursiveOperationManagerOptions options)
                : RecursiveOperationManager(std::move(options)) {}
        };
        auto instance = std::make_shared<MakeSharedEnabler>(std::move(options));
        instance->registerLocalRpcMethods();
        return instance;
    }

    ~RecursiveOperationManager() override = default;

    folly::coro::Task<RecursiveOperationResult> execute(
        DhtAddress targetId,
        RecursiveOperation operation,
        std::optional<DhtAddress> excludedPeer = std::nullopt,
        bool waitForCompletion = true) {
        {
            std::scoped_lock lock(this->mutex);
            if (this->stopped) {
                co_return RecursiveOperationResult{};
            }
        }
        auto self = this->sharedFromThis<RecursiveOperationManager>();
        const size_t waitedRoutingPathCompletions =
            this->options.connectionsView.getConnectionCount() > 1 ? 2 : 1;
        auto session = RecursiveOperationSession::newInstance(
            RecursiveOperationSessionOptions{
                .transport = this->options.sessionTransport,
                .targetId = targetId,
                .localPeerDescriptor = this->options.localPeerDescriptor,
                .waitedRoutingPathCompletions = waitedRoutingPathCompletions,
                .operation = operation,
                .doRouteRequest =
                    [self, excludedPeer](const RouteMessageWrapper& message) {
                        return self->doRouteRequest(message, excludedPeer);
                    }});
        if (this->options.connectionsView.getConnectionCount() == 0) {
            const auto dataEntries =
                this->options.localDataStore.values(targetId);
            session->onResponseReceived(
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->options.localPeerDescriptor),
                {this->options.localPeerDescriptor},
                {this->options.localPeerDescriptor},
                dataEntries,
                true);
            // TS returns without stopping the session; its GC keeps the
            // still-subscribed transport listener alive. Here the session
            // dies with this frame, so the listeners MUST be detached
            // first — skipping this leaves a dangling Message handler on
            // the transport (use-after-free on the next routed message).
            session->stop();
            {
                std::scoped_lock lock(this->mutex);
                this->retiredSessions.push_back(session);
                while (this->retiredSessions.size() > retiredSessionCapacity) {
                    this->retiredSessions.pop_front();
                }
            }
            co_return session->getResults();
        }
        {
            std::scoped_lock lock(this->mutex);
            this->ongoingSessions.emplace(session->getId(), session);
        }
        if (waitForCompletion) {
            session->start(this->options.serviceId);
            if (!session->isCompletionEmitted()) {
                try {
                    co_await streamr::utils::waitForEvent<
                        recursiveoperationsessionevents::Completed>(
                        session.get(), recursiveOperationTimeout);
                } catch (const std::exception& err) {
                    SLogger::debug("start failed " + std::string(err.what()));
                }
            }
        } else {
            session->start(this->options.serviceId);
            // Give the router time to send the delete out.
            co_await folly::coro::sleep(deleteWaitTime);
        }
        if (operation == RecursiveOperation::FETCH_DATA) {
            const auto dataEntries =
                this->options.localDataStore.values(targetId);
            if (!dataEntries.empty()) {
                this->sendResponse(
                    {this->options.localPeerDescriptor},
                    this->options.localPeerDescriptor,
                    ServiceID{session->getId()},
                    {},
                    dataEntries,
                    true);
            }
        } else if (operation == RecursiveOperation::DELETE_DATA) {
            this->options.localDataStore.markAsDeleted(
                targetId,
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->options.localPeerDescriptor));
        }
        {
            std::scoped_lock lock(this->mutex);
            this->ongoingSessions.erase(session->getId());
        }
        session->stop();
        {
            std::scoped_lock lock(this->mutex);
            this->retiredSessions.push_back(session);
            while (this->retiredSessions.size() > retiredSessionCapacity) {
                this->retiredSessions.pop_front();
            }
        }
        co_return session->getResults();
    }

    void stop() {
        {
            std::scoped_lock lock(this->mutex);
            this->stopped = true;
            for (auto& [sessionId, session] : this->ongoingSessions) {
                session->stop();
            }
            this->ongoingSessions.clear();
            this->retiredSessions.clear();
        }
        // Drain the detached response sends BEFORE destroying the retired
        // communicators: a draining task may still retire its communicator
        // into the list (it takes this->mutex to do so, which is why the
        // close runs outside the lock). close() blocks — bounded by
        // sessionRpcTimeout, the cap on every send task — and runs on the
        // owner's thread (DhtNode::stop() calls this while the session
        // transport is still alive), never on a pool worker, so the join
        // may safely wait for tasks running on the pool. sendResponse
        // calls racing this close are dropped by the scope's gate, which
        // is correct for a fire-and-forget response after stop.
        this->sendResponseScope.close();
        std::vector<std::shared_ptr<ListeningRpcCommunicator>> retiredToDestroy;
        {
            std::scoped_lock lock(this->mutex);
            retiredToDestroy.swap(this->retiredSessionCommunicators);
        }
        // Destroyed outside the mutex on the stopper's thread (never a
        // pool worker), where the destructors' scope joins may safely wait
        // for the send-task tails to settle on the pool.
        retiredToDestroy.clear();
    }
};

} // namespace streamr::dht::recursiveoperation
