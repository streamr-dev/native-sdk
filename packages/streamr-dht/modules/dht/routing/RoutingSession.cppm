// Module streamr.dht.RoutingSession
// Ported from packages/dht/src/dht/routing/RoutingSession.ts
// (v103.8.0-rc.3). One session drives a single routed message towards its
// target: it pulls the closest routable contacts (from the shared
// RoutingTablesCache), sends the message to up to `parallelism` of them at
// a time, and retries on failure until it succeeds, partially succeeds, or
// runs out of contacts.
//
// C++ threading (TS is single-threaded and uses setImmediate): the message
// sends are dispatched onto the worker executor supplied by the Router, so
// the per-hop send orchestration and its retries run off the RPC/transport
// thread. State is guarded by one recursive mutex, the session is owned via
// shared_ptr so an in-flight send can pin it, and an AbortController lets
// stop() cancel outstanding sends promptly (the RPC layer detaches on
// cancel).
//
// RoutingRemoteContact lives in its own module (streamr.dht.
// RoutingRemoteContact) to break the RoutingTablesCache <-> RoutingSession
// import cycle that TS tolerates but C++ modules cannot.
module;
#include <new>



export module streamr.dht.RoutingSession;

import std;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.ExecutorHelper;
import streamr.utils.Uuid;
import streamr.logger.SLogger;
import streamr.protorpc.RpcCommunicator;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.getPreviousPeer;
import streamr.dht.RoutingRemoteContact;
import streamr.dht.RoutingTablesCache;
import streamr.dht.SortedContactList;

// Hoisted from the former header (file scope, NOT exported).
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::AbortController;
using streamr::utils::EnableSharedFromThis;
using streamr::utils::Uuid;

export namespace streamr::dht::routing {

using ::dht::PeerDescriptor;
using ::dht::RouteMessageWrapper;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::contact::SortedContactList;
using streamr::dht::contact::SortedContactListOptions;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::protorpc::RpcCommunicator;

enum class RoutingMode { ROUTE, FORWARD, RECURSIVE };

namespace routingsessionevents {
// A peer answered a routeMessage call with a success ack.
struct RoutingSucceeded : Event<> {};
struct PartialSuccess : Event<> {};
// All candidates were tried and none answered with a success ack.
struct RoutingFailed : Event<> {};
struct Stopped : Event<> {};
} // namespace routingsessionevents

using RoutingSessionEvents = std::tuple<
    routingsessionevents::RoutingSucceeded,
    routingsessionevents::PartialSuccess,
    routingsessionevents::RoutingFailed,
    routingsessionevents::Stopped>;

struct RoutingSessionOptions {
    RpcCommunicator<DhtCallContext>& rpcCommunicator;
    PeerDescriptor localPeerDescriptor;
    RouteMessageWrapper routedMessage;
    std::size_t parallelism;
    RoutingMode mode;
    std::set<DhtAddress> excludedNodeIds;
    RoutingTablesCache& routingTablesCache;
    std::function<std::vector<PeerDescriptor>()> getConnections;
    // Worker executor the sends are dispatched onto (owned by the Router).
    folly::CPUThreadPoolExecutor* executor;
};

class RoutingSession : public EventEmitter<RoutingSessionEvents>,
                       public EnableSharedFromThis {
private:
    static constexpr int maxFailedHops = 2;
    static constexpr int routingTableMaxSize = 5;

    RoutingSessionOptions options;
    std::string sessionId = Uuid::v4();
    std::recursive_mutex mutex;
    AbortController abortController;
    std::set<DhtAddress> ongoingRequests;
    std::set<DhtAddress> contactedPeers;
    int failedHopCounter = 0;
    int successfulHopCounter = 0;
    bool stopped = false;

    explicit RoutingSession(RoutingSessionOptions options)
        : options(std::move(options)) {}

    void emitFailure() {
        if (this->successfulHopCounter >= 1) {
            this->emit<routingsessionevents::PartialSuccess>();
        } else {
            this->emit<routingsessionevents::RoutingFailed>();
        }
    }

    void onRequestFailed(const DhtAddress& nodeId) {
        std::scoped_lock lock(this->mutex);
        SLogger::trace("onRequestFailed() sessionId: " + this->sessionId);
        if (this->stopped) {
            return;
        }
        this->ongoingRequests.erase(nodeId);
        this->deleteParallelRootIfSource(nodeId);
        this->failedHopCounter += 1;
        if (this->failedHopCounter >= maxFailedHops) {
            this->emitFailure();
            return;
        }
        const auto contacts = this->updateAndGetRoutablePeers();
        if (contacts.empty() && this->ongoingRequests.empty()) {
            this->emitFailure();
        } else {
            this->sendMoreRequests(contacts);
        }
    }

    void onRequestSucceeded() {
        std::scoped_lock lock(this->mutex);
        SLogger::trace("onRequestSucceeded() sessionId: " + this->sessionId);
        if (this->stopped) {
            return;
        }
        this->successfulHopCounter += 1;
        if (this->successfulHopCounter >=
            static_cast<int>(this->options.parallelism)) {
            this->emit<routingsessionevents::RoutingSucceeded>();
            return;
        }
        const auto contacts = this->updateAndGetRoutablePeers();
        if (contacts.empty()) {
            this->emit<routingsessionevents::RoutingSucceeded>();
        } else if (this->ongoingRequests.empty()) {
            this->sendMoreRequests(contacts);
        }
    }

    folly::coro::Task<bool> sendRouteMessageRequest(
        std::shared_ptr<RoutingRemoteContact> contact) {
        {
            std::scoped_lock lock(this->mutex);
            if (this->stopped) {
                co_return false;
            }
        }
        RouteMessageWrapper msg = this->options.routedMessage;
        *msg.add_routingpath() = this->options.localPeerDescriptor;
        if (this->options.mode == RoutingMode::FORWARD) {
            co_return co_await contact->getRouterRpcRemote().forwardMessage(
                msg);
        }
        if (this->options.mode == RoutingMode::RECURSIVE) {
            co_return co_await contact->getRecursiveOperationRpcRemote()
                .routeRequest(msg);
        }
        co_return co_await contact->getRouterRpcRemote().routeMessage(msg);
    }

    // Dispatches one send onto the worker executor (TS uses setImmediate).
    // The session is pinned for the send's lifetime and the call is made
    // cancellable so stop() can abandon it promptly.
    void dispatchSend(const std::shared_ptr<RoutingRemoteContact>& contact) {
        auto self = this->sharedFromThis<RoutingSession>();
        const DhtAddress nodeId = contact->getNodeId();
        // co_invoke keeps the lambda (and its captured self/contact) alive
        // for the coroutine's whole lifetime; invoking the lambda directly
        // would destroy the closure while the coroutine is still suspended.
        streamr::utils::co_withExecutor(
            this->options.executor,
            folly::coro::co_invoke(
                [self, contact, nodeId]() -> folly::coro::Task<void> {
                    bool succeeded = false;
                    try {
                        succeeded =
                            co_await streamr::utils::co_withCancellation(
                                self->abortController.getSignal()
                                    .getCancellationToken(),
                                self->sendRouteMessageRequest(contact));
                    } catch (const std::exception& err) {
                        SLogger::debug(
                            "Unable to route message ",
                            std::string(err.what()));
                        co_return;
                    }
                    if (succeeded) {
                        self->onRequestSucceeded();
                    } else {
                        self->onRequestFailed(nodeId);
                    }
                }))
            .start();
    }

    void addParallelRootIfSource(const DhtAddress& nodeId) {
        if (this->options.mode == RoutingMode::RECURSIVE &&
            Identifiers::areEqualPeerDescriptors(
                this->options.localPeerDescriptor,
                this->options.routedMessage.sourcepeer())) {
            this->options.routedMessage.add_parallelrootnodeids(
                std::string(nodeId));
        }
    }

    void deleteParallelRootIfSource(const DhtAddress& nodeId) {
        if (this->options.mode == RoutingMode::RECURSIVE &&
            Identifiers::areEqualPeerDescriptors(
                this->options.localPeerDescriptor,
                this->options.routedMessage.sourcepeer())) {
            auto* const roots =
                this->options.routedMessage.mutable_parallelrootnodeids();
            for (int i = roots->size() - 1; i >= 0; --i) {
                if (roots->Get(i) == std::string(nodeId)) {
                    roots->DeleteSubrange(i, 1);
                }
            }
        }
    }

public:
    [[nodiscard]] static std::shared_ptr<RoutingSession> newInstance(
        RoutingSessionOptions options) {
        struct MakeSharedEnabler : public RoutingSession {
            explicit MakeSharedEnabler(RoutingSessionOptions options)
                : RoutingSession(std::move(options)) {}
        };
        return std::make_shared<MakeSharedEnabler>(std::move(options));
    }

    ~RoutingSession() override = default;

    [[nodiscard]] const std::string& getSessionId() const {
        return this->sessionId;
    }

    std::vector<std::shared_ptr<RoutingRemoteContact>>
    updateAndGetRoutablePeers() {
        std::scoped_lock lock(this->mutex);
        SLogger::trace(
            "updateAndGetRoutablePeers() sessionId: " + this->sessionId);
        const auto previousPeer = getPreviousPeer(this->options.routedMessage);
        const std::optional<DhtAddress> previousId = previousPeer.has_value()
            ? std::optional<
                  DhtAddress>{Identifiers::getNodeIdFromPeerDescriptor(
                  previousPeer.value())}
            : std::nullopt;
        const DhtAddress targetId = Identifiers::getDhtAddressFromRaw(
            DhtAddressRaw{this->options.routedMessage.target()});

        RoutingTable routingTable;
        if (this->options.routingTablesCache.has(targetId, previousId) &&
            this->options.routingTablesCache.get(targetId, previousId)
                    ->getSize() > 0) {
            routingTable =
                this->options.routingTablesCache.get(targetId, previousId);
        } else {
            routingTable =
                std::make_shared<SortedContactList<RoutingRemoteContact>>(
                    SortedContactListOptions{
                        .referenceId = targetId,
                        .allowToContainReferenceId = true,
                        .maxSize = static_cast<std::size_t>(routingTableMaxSize),
                        .nodeIdDistanceLimit = previousId});
            std::vector<std::shared_ptr<RoutingRemoteContact>> contacts;
            for (const auto& peer : this->options.getConnections()) {
                contacts.push_back(
                    std::make_shared<RoutingRemoteContact>(
                        peer,
                        this->options.localPeerDescriptor,
                        this->options.rpcCommunicator));
            }
            routingTable->addContacts(contacts);
            this->options.routingTablesCache.set(
                targetId, routingTable, previousId);
        }
        std::vector<std::shared_ptr<RoutingRemoteContact>> result;
        for (const auto& contact : routingTable->getClosestContacts()) {
            const DhtAddress contactId = contact->getNodeId();
            if (!this->contactedPeers.contains(contactId) &&
                !this->options.excludedNodeIds.contains(contactId)) {
                result.push_back(contact);
            }
        }
        return result;
    }

    void sendMoreRequests(
        std::vector<std::shared_ptr<RoutingRemoteContact>> uncontacted) {
        std::scoped_lock lock(this->mutex);
        SLogger::trace("sendMoreRequests() sessionId: " + this->sessionId);
        if (this->stopped) {
            return;
        }
        if (uncontacted.empty()) {
            this->emitFailure();
            return;
        }
        std::size_t index = 0;
        while (this->ongoingRequests.size() < this->options.parallelism &&
               index < uncontacted.size() && !this->stopped) {
            const auto nextPeer = uncontacted[index];
            ++index;
            const DhtAddress nodeId = nextPeer->getNodeId();
            this->contactedPeers.insert(nodeId);
            this->ongoingRequests.insert(nodeId);
            this->addParallelRootIfSource(nodeId);
            this->dispatchSend(nextPeer);
        }
    }

    void stop() {
        std::scoped_lock lock(this->mutex);
        this->stopped = true;
        this->abortController.abort();
        this->emit<routingsessionevents::Stopped>();
        this->removeAllListeners();
    }
};

} // namespace streamr::dht::routing
