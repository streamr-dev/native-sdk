// Module streamr.dht.Router
// Ported from packages/dht/src/dht/routing/Router.ts (v103.8.0-rc.3). The
// Router receives routeMessage/forwardMessage RPCs, decides the next hops
// (via a RoutingSession over the cached routing tables), and forwards the
// message, deduplicating with a DuplicateDetector.
//
// C++ threading (the routing operations proved heavy in TS): the Router
// owns a dedicated single-thread worker executor and runs ALL routing work
// on it — the routeMessage/forwardMessage RPC handlers co_await onto the
// worker (async, see below), the session send-completions resume on the
// worker, and the cross-thread entry points (onNodeConnected/Disconnected,
// resetCache, stop, the duplicate-detector accessors) dispatch onto it too.
// Because every access to the routing state (routing-tables cache, ongoing
// sessions, forwarding table, duplicate detector) happens on that one
// worker thread, the state needs no additional locks, and the heavy routing
// computation never runs on the transport/RPC delivery thread. A single
// worker serializes routing; scaling it to a pool would require finer-
// grained locking of the cache and sessions.
//
// Follow-up to Phase A4 (owner priority model — our own traffic is
// priority 1, routing is best-effort "whenever we have time"): the
// routeMessage/forwardMessage handlers are registered as ASYNC proto-rpc
// handlers (registerRpcMethodAsync) that co_await the routing worker
// instead of blockingWait-ing on it. The delivery coroutine therefore
// SUSPENDS (it does not block) and the shared delivery thread returns to
// our own incoming traffic immediately; the ack is produced on the worker
// and sent later. The worker is additionally given a low OS thread priority
// (a positive nice value, best-effort per-platform) so routing yields the
// CPU to our own work under contention.
module;

#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

export module streamr.dht.Router;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.utils.AbortController;
import streamr.utils.AbortableTimers;
import streamr.utils.CoroutineHelper;
import streamr.utils.SharedExecutors;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.ExecutorHelper;
import streamr.utils.Uuid;
import streamr.logger.SLogger;
import streamr.protorpc.RpcCommunicator;
import streamr.dht.DhtCallContext;
import streamr.dht.DuplicateDetector;
import streamr.dht.Identifiers;
import streamr.dht.RouterRpcLocal;
import streamr.dht.RoutingRemoteContact;
import streamr.dht.RoutingSession;
import streamr.dht.RoutingTablesCache;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::AbortableTimers;
using streamr::utils::AbortController;
using streamr::utils::EnableSharedFromThis;
using streamr::utils::Uuid;

export namespace streamr::dht::routing {

using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageError;
using ::dht::RouteMessageWrapper;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::routing::DuplicateDetector;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::protorpc::RpcCommunicator;

struct RouterOptions {
    RpcCommunicator<DhtCallContext>& rpcCommunicator;
    PeerDescriptor localPeerDescriptor;
    std::function<void(const Message&)> handleMessage;
    std::function<std::vector<PeerDescriptor>()> getConnections;
};

class Router : public EnableSharedFromThis {
private:
    static constexpr size_t duplicateDetectorSize = 10000;
    static constexpr std::chrono::milliseconds forwardingEntryTtl{10000};
    static constexpr std::chrono::milliseconds sessionTimeout{10000};

    struct ForwardingTableEntry {
        std::vector<PeerDescriptor> peerDescriptors;
        std::shared_ptr<AbortController> timeoutAbort;
    };

    RouterOptions options;
    // Serial view of the shared LOW-PRIORITY pool (formerly a private
    // single-thread nice-10 pool — see streamr.utils.SharedExecutors, which
    // keeps the "routing runs whenever we have time" priority). The SERIAL
    // ordering is load-bearing: forwardingTable, ongoingRoutingSessions and
    // `stopped` are synchronized by everything running one-at-a-time on
    // this executor, not by a mutex.
    streamr::utils::SharedSerialExecutor routingExecutor{
        streamr::utils::SharedExecutors::background()};
    AbortController abortController; // cancels the session-cleanup timeouts
    std::map<DhtAddress, ForwardingTableEntry> forwardingTable;
    RoutingTablesCache routingTablesCache;
    std::map<std::string, std::shared_ptr<RoutingSession>>
        ongoingRoutingSessions;
    DuplicateDetector duplicateRequestDetector{duplicateDetectorSize};
    bool stopped = false;
    std::unique_ptr<RouterRpcLocal> routerRpcLocal;

    explicit Router(RouterOptions options) : options(std::move(options)) {}

    // Runs `fn` on the routing worker and blocks for its result. Callers
    // must NOT already be on this serial executor (that would self-deadlock
    // it); every caller here is an off-worker entry point.
    template <typename Fn>
    auto runOnWorker(Fn fn) -> decltype(fn()) {
        using Ret = decltype(fn());
        return streamr::utils::blockingWait(
            streamr::utils::co_withExecutor(
                &this->routingExecutor,
                folly::coro::co_invoke(
                    [fn = std::move(fn)]() -> folly::coro::Task<Ret> {
                        co_return fn();
                    })));
    }

    void registerLocalRpcMethods() {
        this->routerRpcLocal =
            std::make_unique<RouterRpcLocal>(RouterRpcLocalOptions{
                .doRouteMessage =
                    [this](
                        const RouteMessageWrapper& routedMessage,
                        std::optional<RoutingMode> mode) {
                        return this->doRouteMessage(
                            routedMessage,
                            mode.value_or(RoutingMode::ROUTE),
                            std::nullopt);
                    },
                .setForwardingEntries =
                    [this](const RouteMessageWrapper& routedMessage) {
                        this->setForwardingEntries(routedMessage);
                    },
                .handleMessage = this->options.handleMessage,
                .duplicateRequestDetector = this->duplicateRequestDetector,
                .localPeerDescriptor = this->options.localPeerDescriptor});

        std::weak_ptr<Router> weakSelf = this->sharedFromThis<Router>();
        // routeMessage/forwardMessage are async handlers: they co_await the
        // routing worker so the delivery coroutine SUSPENDS (does not block
        // the shared delivery thread) until the ack is ready. `self` (a
        // shared_ptr held in the coroutine frame) keeps the Router and its
        // routingExecutor alive across the suspension. The inner worker task
        // captures its inputs by value, so it is independent of the outer
        // frame. Wire semantics are unchanged from the previous blocking
        // handlers (same STOPPED / NO_TARGETS / DUPLICATE acks).
        this->options.rpcCommunicator.template registerRpcMethodAsync<
            RouteMessageWrapper,
            RouteMessageAck>(
            "routeMessage",
            [weakSelf](
                const RouteMessageWrapper& routedMessage,
                const DhtCallContext& callContext)
                -> folly::coro::Task<RouteMessageAck> {
                auto self = weakSelf.lock();
                if (!self) {
                    co_return createRouteMessageAck(
                        routedMessage, RouteMessageError::STOPPED);
                }
                co_return co_await streamr::utils::co_withExecutor(
                    &self->routingExecutor,
                    folly::coro::co_invoke(
                        [self,
                         routedMessage,
                         callContext]() -> folly::coro::Task<RouteMessageAck> {
                            if (self->stopped) {
                                co_return createRouteMessageAck(
                                    routedMessage, RouteMessageError::STOPPED);
                            }
                            co_return self->routerRpcLocal->routeMessage(
                                routedMessage, callContext);
                        }));
            });
        this->options.rpcCommunicator.template registerRpcMethodAsync<
            RouteMessageWrapper,
            RouteMessageAck>(
            "forwardMessage",
            [weakSelf](
                const RouteMessageWrapper& forwardMessage,
                const DhtCallContext& callContext)
                -> folly::coro::Task<RouteMessageAck> {
                auto self = weakSelf.lock();
                if (!self) {
                    co_return createRouteMessageAck(
                        forwardMessage, RouteMessageError::STOPPED);
                }
                co_return co_await streamr::utils::co_withExecutor(
                    &self->routingExecutor,
                    folly::coro::co_invoke(
                        [self,
                         forwardMessage,
                         callContext]() -> folly::coro::Task<RouteMessageAck> {
                            if (self->stopped) {
                                co_return createRouteMessageAck(
                                    forwardMessage, RouteMessageError::STOPPED);
                            }
                            co_return self->routerRpcLocal->forwardMessage(
                                forwardMessage, callContext);
                        }));
            });
    }

    // Runs on the routing worker.
    void cleanupSession(const std::string& sessionId) {
        const auto it = this->ongoingRoutingSessions.find(sessionId);
        if (it == this->ongoingRoutingSessions.end()) {
            return;
        }
        auto session = it->second;
        this->ongoingRoutingSessions.erase(it);
        session->stop();
    }

    std::shared_ptr<RoutingSession> createRoutingSession(
        const RouteMessageWrapper& routedMessage,
        RoutingMode mode,
        std::optional<DhtAddress> excludedNode) {
        std::set<DhtAddress> excludedNodeIds;
        for (const auto& descriptor : routedMessage.routingpath()) {
            excludedNodeIds.insert(
                Identifiers::getNodeIdFromPeerDescriptor(descriptor));
        }
        if (excludedNode.has_value()) {
            excludedNodeIds.insert(excludedNode.value());
        }
        for (const auto& nodeId : routedMessage.parallelrootnodeids()) {
            excludedNodeIds.insert(DhtAddress{nodeId});
        }
        constexpr size_t sourceParallelism = 2;
        constexpr size_t forwardParallelism = 1;
        const size_t parallelism =
            Identifiers::areEqualPeerDescriptors(
                this->options.localPeerDescriptor, routedMessage.sourcepeer())
            ? sourceParallelism
            : forwardParallelism;
        return RoutingSession::newInstance(
            RoutingSessionOptions{
                .rpcCommunicator = this->options.rpcCommunicator,
                .localPeerDescriptor = this->options.localPeerDescriptor,
                .routedMessage = routedMessage,
                .parallelism = parallelism,
                .mode = mode,
                .excludedNodeIds = excludedNodeIds,
                .routingTablesCache = this->routingTablesCache,
                .getConnections = this->options.getConnections,
                .executor = &this->routingExecutor});
    }

    // Runs on the routing worker.
    void setForwardingEntries(const RouteMessageWrapper& routedMessage) {
        std::vector<PeerDescriptor> reachableThroughWithoutSelf;
        for (const auto& peer : routedMessage.reachablethrough()) {
            if (!Identifiers::areEqualPeerDescriptors(
                    peer, this->options.localPeerDescriptor)) {
                reachableThroughWithoutSelf.push_back(peer);
            }
        }
        if (reachableThroughWithoutSelf.empty()) {
            return;
        }
        const DhtAddress sourceNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(
                routedMessage.sourcepeer());
        const auto existing = this->forwardingTable.find(sourceNodeId);
        if (existing != this->forwardingTable.end()) {
            existing->second.timeoutAbort->abort();
            this->forwardingTable.erase(existing);
        }
        auto timeoutAbort = std::make_shared<AbortController>();
        auto self = this->sharedFromThis<Router>();
        AbortableTimers::setAbortableTimeout(
            [self, sourceNodeId]() {
                streamr::utils::co_withExecutor(
                    &self->routingExecutor,
                    folly::coro::co_invoke(
                        [self, sourceNodeId]() -> folly::coro::Task<void> {
                            self->forwardingTable.erase(sourceNodeId);
                            co_return;
                        }))
                    .start();
            },
            forwardingEntryTtl,
            timeoutAbort->getSignal());
        this->forwardingTable.emplace(
            sourceNodeId,
            ForwardingTableEntry{
                std::move(reachableThroughWithoutSelf),
                std::move(timeoutAbort)});
    }

public:
    [[nodiscard]] static std::shared_ptr<Router> newInstance(
        RouterOptions options) {
        struct MakeSharedEnabler : public Router {
            explicit MakeSharedEnabler(RouterOptions options)
                : Router(std::move(options)) {}
        };
        auto instance = std::make_shared<MakeSharedEnabler>(std::move(options));
        instance->registerLocalRpcMethods();
        return instance;
    }

    ~Router() override = default;

    // Runs on the routing worker (called from the RPC handlers and, via
    // runOnWorker, from send()).
    RouteMessageAck doRouteMessage(
        const RouteMessageWrapper& routedMessage,
        RoutingMode mode = RoutingMode::ROUTE,
        std::optional<DhtAddress> excludedPeer = std::nullopt) {
        if (this->stopped) {
            return createRouteMessageAck(
                routedMessage, RouteMessageError::STOPPED);
        }
        auto session =
            this->createRoutingSession(routedMessage, mode, excludedPeer);
        const auto contacts = session->updateAndGetRoutablePeers();
        if (contacts.empty()) {
            return createRouteMessageAck(
                routedMessage, RouteMessageError::NO_TARGETS);
        }
        const std::string sessionId = session->getSessionId();
        this->ongoingRoutingSessions.emplace(sessionId, session);
        auto self = this->sharedFromThis<Router>();
        auto onDone = [self, sessionId]() { self->cleanupSession(sessionId); };
        session->once<routingsessionevents::RoutingSucceeded>(
            [onDone]() { onDone(); });
        session->once<routingsessionevents::PartialSuccess>(
            [onDone]() { onDone(); });
        session->once<routingsessionevents::RoutingFailed>(
            [onDone]() { onDone(); });
        session->once<routingsessionevents::Stopped>([onDone]() { onDone(); });
        AbortableTimers::setAbortableTimeout(
            [self, sessionId]() {
                streamr::utils::co_withExecutor(
                    &self->routingExecutor,
                    folly::coro::co_invoke(
                        [self, sessionId]() -> folly::coro::Task<void> {
                            self->cleanupSession(sessionId);
                            co_return;
                        }))
                    .start();
            },
            sessionTimeout,
            this->abortController.getSignal());
        session->sendMoreRequests(contacts);
        return createRouteMessageAck(routedMessage);
    }

    void send(
        const Message& msg,
        const std::vector<PeerDescriptor>& reachableThrough) {
        auto self = this->sharedFromThis<Router>();
        Message message = msg;
        *message.mutable_sourcedescriptor() = this->options.localPeerDescriptor;
        this->runOnWorker([&]() -> std::monostate {
            const DhtAddress targetNodeId =
                Identifiers::getNodeIdFromPeerDescriptor(
                    message.targetdescriptor());
            const auto forwardingEntry =
                this->forwardingTable.find(targetNodeId);
            RouteMessageWrapper routedMessage;
            *routedMessage.mutable_message() = message;
            routedMessage.set_requestid(Uuid::v4());
            *routedMessage.mutable_sourcepeer() =
                this->options.localPeerDescriptor;
            for (const auto& peer : reachableThrough) {
                *routedMessage.add_reachablethrough() = peer;
            }
            RoutingMode mode = RoutingMode::ROUTE;
            if (forwardingEntry != this->forwardingTable.end() &&
                !forwardingEntry->second.peerDescriptors.empty()) {
                routedMessage.set_target(
                    forwardingEntry->second.peerDescriptors[0].nodeid());
                mode = RoutingMode::FORWARD;
            } else {
                routedMessage.set_target(message.targetdescriptor().nodeid());
            }
            this->doRouteMessage(routedMessage, mode, std::nullopt);
            return std::monostate{};
        });
    }

    [[nodiscard]] bool isMostLikelyDuplicate(const std::string& requestId) {
        return this->runOnWorker([&]() {
            return this->duplicateRequestDetector.isMostLikelyDuplicate(
                requestId);
        });
    }

    void addToDuplicateDetector(const std::string& requestId) {
        this->runOnWorker([&]() -> std::monostate {
            this->duplicateRequestDetector.add(requestId);
            return std::monostate{};
        });
    }

    void onNodeConnected(const PeerDescriptor& peerDescriptor) {
        this->runOnWorker([&]() -> std::monostate {
            auto remote = std::make_shared<RoutingRemoteContact>(
                peerDescriptor,
                this->options.localPeerDescriptor,
                this->options.rpcCommunicator);
            this->routingTablesCache.onNodeConnected(remote);
            return std::monostate{};
        });
    }

    void onNodeDisconnected(const PeerDescriptor& peerDescriptor) {
        this->runOnWorker([&]() -> std::monostate {
            this->routingTablesCache.onNodeDisconnected(
                Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor));
            return std::monostate{};
        });
    }

    void resetCache() {
        this->runOnWorker([&]() -> std::monostate {
            this->routingTablesCache.reset();
            return std::monostate{};
        });
    }

    void stop() {
        this->runOnWorker([&]() -> std::monostate {
            this->stopped = true;
            auto sessions = this->ongoingRoutingSessions;
            this->ongoingRoutingSessions.clear();
            for (auto& [sessionId, session] : sessions) {
                session->stop();
            }
            for (auto& [nodeId, entry] : this->forwardingTable) {
                entry.timeoutAbort->abort();
            }
            this->forwardingTable.clear();
            this->duplicateRequestDetector.clear();
            this->routingTablesCache.reset();
            return std::monostate{};
        });
        this->abortController.abort();
    }
};

} // namespace streamr::dht::routing
