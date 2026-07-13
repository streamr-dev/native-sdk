// Module streamr.dht.PeerDiscovery
// Ported from packages/dht/src/dht/discovery/PeerDiscovery.ts
// (v103.8.0-rc.3). Orchestrates joining the DHT: for each entry point it runs
// a DiscoverySession toward the local id (plus, optionally, a second session
// toward the bit-flipped "distant" id), retries the join when no neighbours
// are found, and once joined keeps a periodic recovery task refreshing the
// neighbour set. joinRing does the same over the ring topology.
//
// PeerDiscovery is shared_ptr-owned (EnableSharedFromThis): the retry timeout
// and the recovery interval are detached and pin `self` for their lifetime,
// and they run on a dedicated worker executor so this 2nd-class maintenance
// work never occupies a caller/delivery thread. Both are torn down by the
// join abort signal (the same one DhtNode aborts on stop).
//
// The join fans out over entry points with collectAllRange; the shared
// contactedPeers sets are mutated across those sibling sessions. That is safe
// under cooperative (single-driver) execution, which is how the join is
// driven; running the fan-out on a multi-threaded executor would need the
// shared sets synchronised. This is exercised by the phase A8 integration
// tests (DhtJoinPeerDiscovery, MultipleEntryPointJoining).
module;

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <coroutine> // IWYU pragma: keep

export module streamr.dht.PeerDiscovery;

import streamr.dht.protos;

import streamr.utils.AbortableTimers;
import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.SharedExecutors;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.ExecutorHelper;
import streamr.utils.scheduleAtInterval;
import streamr.logger.SLogger;
import streamr.dht.ConnectionLocker;
import streamr.dht.ConnectionLockStates;
import streamr.dht.consts;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.DiscoverySession;
import streamr.dht.getClosestNodes;
import streamr.dht.Identifiers;
import streamr.dht.PeerManager;
import streamr.dht.RingDiscoverySession;
import streamr.dht.ringIdentifiers;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::AbortableTimers;
using streamr::utils::AbortSignal;
using streamr::utils::EnableSharedFromThis;
using streamr::utils::scheduleAtInterval;

export namespace streamr::dht::discovery {

using ::dht::PeerDescriptor;
using streamr::dht::CONTROL_LAYER_NODE_SERVICE_ID;
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::DhtNodeRpcRemote;
using streamr::dht::Identifiers;
using streamr::dht::PeerManager;
using streamr::dht::ServiceID;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::LockID;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::contact::GetClosestNodesOptions;
using streamr::dht::contact::getRingIdRawFromPeerDescriptor;
using streamr::dht::contact::RingIdRaw;

// The bit-flipped id: seeds a discovery session far across the keyspace so the
// join learns about distant parts of the network, not just the local vicinity.
inline DhtAddress createDistantDhtAddress(const DhtAddress& address) {
    const auto raw = Identifiers::getRawFromDhtAddress(address);
    std::string flipped;
    flipped.reserve(raw.size());
    for (const char byte : raw) {
        flipped.push_back(static_cast<char>(~static_cast<unsigned char>(byte)));
    }
    return Identifiers::getDhtAddressFromRaw(DhtAddressRaw{flipped});
}

struct PeerDiscoveryOptions {
    PeerDescriptor localPeerDescriptor;
    size_t joinNoProgressLimit;
    ServiceID serviceId;
    size_t parallelism;
    std::chrono::milliseconds joinTimeout;
    std::shared_ptr<ConnectionLocker> connectionLocker; // may be null
    PeerManager& peerManager;
    AbortSignal& abortSignal;
    std::function<std::shared_ptr<DhtNodeRpcRemote>(const PeerDescriptor&)>
        createDhtNodeRpcRemote;
};

// The distant-join arm of joinDht (TS models it as a discriminated union).
struct DistantJoinConfig {
    bool enabled;
    std::set<DhtAddress> contactedPeers; // meaningful only when enabled
};

class PeerDiscovery : public EnableSharedFromThis {
private:
    // TS uses these literals inline (with TODOs to name them); named here.
    static constexpr std::chrono::milliseconds rejoinDelay{1000};
    static constexpr std::chrono::milliseconds rejoinRetryDelay{5000};
    static constexpr std::chrono::milliseconds recoveryInterval{60000};

    std::map<std::string, std::shared_ptr<DiscoverySession>>
        ongoingDiscoverySessions;
    std::map<std::string, std::shared_ptr<RingDiscoverySession>>
        ongoingRingDiscoverySessions;
    bool rejoinOngoing = false;
    bool joinCalled = false;
    bool recoveryIntervalStarted = false;
    std::recursive_mutex mutex;
    // Recovery / rejoin maintenance runs here — 2nd-class work kept off the
    // caller/delivery threads. Serial view of the shared worker pool
    // (formerly a private single-thread pool — see
    // streamr.utils.SharedExecutors); the detached tasks pin `self`
    // (sharedFromThis) and bail on the abort signal, exactly as before.
    streamr::utils::SharedSerialExecutor recoveryExecutor{
        streamr::utils::SharedExecutors::worker()};
    PeerDiscoveryOptions options;

    explicit PeerDiscovery(PeerDiscoveryOptions options)
        : options(std::move(options)) {}

    [[nodiscard]] bool isStopped() const {
        return this->options.abortSignal.aborted;
    }

    [[nodiscard]] LockID joinLockId() const {
        return LockID{
            static_cast<std::string>(this->options.serviceId) + "::joinDht"};
    }

    [[nodiscard]] std::shared_ptr<DiscoverySession> createSession(
        const DhtAddress& targetId, std::set<DhtAddress>& contactedPeers) {
        return std::make_shared<DiscoverySession>(DiscoverySessionOptions{
            .targetId = targetId,
            .parallelism = this->options.parallelism,
            .noProgressLimit = this->options.joinNoProgressLimit,
            .peerManager = this->options.peerManager,
            .contactedPeers = contactedPeers,
            .abortSignal = this->options.abortSignal,
            .createDhtNodeRpcRemote = this->options.createDhtNodeRpcRemote});
    }

    [[nodiscard]] std::shared_ptr<RingDiscoverySession> createRingSession(
        const RingIdRaw& targetId, std::set<DhtAddress>& contactedPeers) {
        return std::make_shared<RingDiscoverySession>(
            RingDiscoverySessionOptions{
                .targetId = targetId,
                .parallelism = this->options.parallelism,
                .noProgressLimit = this->options.joinNoProgressLimit,
                .peerManager = this->options.peerManager,
                .contactedPeers = contactedPeers,
                .abortSignal = this->options.abortSignal});
    }

    folly::coro::Task<void> runSessions(
        std::vector<std::shared_ptr<DiscoverySession>> sessions,
        PeerDescriptor entryPointDescriptor,
        bool retry) {
        // Snapshot the caller's cancellation token: a join cancelled by
        // its owner's stop() must not schedule a rejoin (the detached
        // rejoin would outlive the intent to stop).
        const auto callerToken =
            co_await streamr::utils::co_currentCancellationToken();
        try {
            for (const auto& session : sessions) {
                {
                    std::scoped_lock lock(this->mutex);
                    this->ongoingDiscoverySessions[session->getId()] = session;
                }
                co_await session->findClosestNodes(this->options.joinTimeout);
            }
        } catch (const std::exception&) {
            SLogger::debug("DHT join timed out");
        }
        if (!this->isStopped() && !callerToken.isCancellationRequested()) {
            if (this->options.peerManager.getNeighborCount() == 0) {
                if (retry) {
                    this->scheduleRejoin(entryPointDescriptor, rejoinDelay);
                }
            } else {
                co_await this->ensureRecoveryIntervalIsRunning();
            }
        }
        {
            std::scoped_lock lock(this->mutex);
            for (const auto& session : sessions) {
                this->ongoingDiscoverySessions.erase(session->getId());
            }
        }
    }

    folly::coro::Task<void> runRingSessions(
        std::vector<std::shared_ptr<RingDiscoverySession>> sessions) {
        try {
            for (const auto& session : sessions) {
                {
                    std::scoped_lock lock(this->mutex);
                    this->ongoingRingDiscoverySessions[session->getId()] =
                        session;
                }
                co_await session->findClosestNodes(this->options.joinTimeout);
            }
        } catch (const std::exception&) {
            SLogger::debug("Ring join timed out");
        }
        {
            std::scoped_lock lock(this->mutex);
            for (const auto& session : sessions) {
                this->ongoingRingDiscoverySessions.erase(session->getId());
            }
        }
    }

    // Detaches a rejoin attempt onto the worker executor after `delay`,
    // cancelled by the abort signal (TS: setAbortableTimeout(() =>
    // rejoinDht(...), delay, signal)).
    void scheduleRejoin(
        const PeerDescriptor& entryPoint, std::chrono::milliseconds delay) {
        auto self = this->sharedFromThis<PeerDiscovery>();
        auto entryPointCopy = entryPoint;
        AbortableTimers::setAbortableTimeout(
            [self, entryPointCopy]() {
                streamr::utils::co_withExecutor(
                    &self->recoveryExecutor,
                    folly::coro::co_invoke(
                        [self, entryPointCopy]() -> folly::coro::Task<void> {
                            co_await self->rejoinDht(entryPointCopy);
                        }))
                    .start();
            },
            delay,
            this->options.abortSignal);
    }

    folly::coro::Task<void> ensureRecoveryIntervalIsRunning() {
        bool shouldStart = false;
        {
            std::scoped_lock lock(this->mutex);
            if (!this->recoveryIntervalStarted) {
                this->recoveryIntervalStarted = true;
                shouldStart = true;
            }
        }
        if (shouldStart) {
            auto self = this->sharedFromThis<PeerDiscovery>();
            co_await scheduleAtInterval(
                [self]() -> folly::coro::Task<void> {
                    co_await self->fetchClosestAndRandomNeighbors();
                },
                recoveryInterval,
                true,
                this->options.abortSignal,
                &this->recoveryExecutor);
        }
    }

    [[nodiscard]] std::vector<PeerDescriptor> getClosestNeighbors(
        const DhtAddress& referenceId, size_t maxCount) {
        std::vector<PeerDescriptor> neighborDescriptors;
        const auto neighbors = this->options.peerManager.getNeighbors();
        neighborDescriptors.reserve(neighbors.size());
        for (const auto& neighbor : neighbors) {
            neighborDescriptors.push_back(neighbor->getPeerDescriptor());
        }
        return getClosestNodes(
            referenceId,
            neighborDescriptors,
            GetClosestNodesOptions{.maxCount = maxCount});
    }

    folly::coro::Task<void> fetchClosestAndRandomNeighbors() {
        if (this->isStopped()) {
            co_return;
        }
        const auto localNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            this->options.localPeerDescriptor);
        const auto nodes =
            this->getClosestNeighbors(localNodeId, this->options.parallelism);
        const auto randomNodes =
            this->getClosestNeighbors(Identifiers::createRandomDhtAddress(), 1);
        auto self = this->sharedFromThis<PeerDiscovery>();
        std::vector<folly::coro::Task<void>> fetches;
        fetches.reserve(nodes.size() + randomNodes.size());
        for (const auto& node : nodes) {
            fetches.push_back(
                folly::coro::co_invoke(
                    [self, node, localNodeId]() -> folly::coro::Task<void> {
                        co_await self->fetchAndAddContacts(node, localNodeId);
                    }));
        }
        for (const auto& node : randomNodes) {
            fetches.push_back(
                folly::coro::co_invoke(
                    [self, node]() -> folly::coro::Task<void> {
                        co_await self->fetchAndAddContacts(
                            node, Identifiers::createRandomDhtAddress());
                    }));
        }
        // collectAllRange, but each fetch swallows its own failure — the TS
        // uses Promise.allSettled, so one failed remote does not abort the
        // rest.
        co_await folly::coro::collectAllRange(std::move(fetches));
    }

    folly::coro::Task<void> fetchAndAddContacts(
        PeerDescriptor node, DhtAddress referenceId) {
        try {
            const auto remote = this->options.createDhtNodeRpcRemote(node);
            const auto contacts = co_await remote->getClosestPeers(referenceId);
            for (const auto& contact : contacts) {
                this->options.peerManager.addContact(contact);
            }
        } catch (const std::exception& e) {
            SLogger::trace(
                "recovery getClosestPeers failed: " + std::string(e.what()));
        }
    }

public:
    [[nodiscard]] static std::shared_ptr<PeerDiscovery> newInstance(
        PeerDiscoveryOptions options) {
        struct MakeSharedEnabler : public PeerDiscovery {
            explicit MakeSharedEnabler(PeerDiscoveryOptions options)
                : PeerDiscovery(std::move(options)) {}
        };
        return std::make_shared<MakeSharedEnabler>(std::move(options));
    }

    ~PeerDiscovery() override = default;

    PeerDiscovery(const PeerDiscovery&) = delete;
    PeerDiscovery& operator=(const PeerDiscovery&) = delete;
    PeerDiscovery(PeerDiscovery&&) = delete;
    PeerDiscovery& operator=(PeerDiscovery&&) = delete;

    folly::coro::Task<void> joinDht(
        std::vector<PeerDescriptor> entryPoints,
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        bool doAdditionalDistantPeerDiscovery = true,
        bool retry = true) {
        std::set<DhtAddress> contactedPeers;
        DistantJoinConfig distantJoinOptions{
            .enabled = doAdditionalDistantPeerDiscovery, .contactedPeers = {}};
        auto self = this->sharedFromThis<PeerDiscovery>();
        std::vector<folly::coro::Task<void>> joins;
        joins.reserve(entryPoints.size());
        for (const auto& entryPoint : entryPoints) {
            joins.push_back(
                folly::coro::co_invoke(
                    [self,
                     entryPoint,
                     &contactedPeers,
                     &distantJoinOptions,
                     retry]() -> folly::coro::Task<void> {
                        co_await self->joinThroughEntryPoint(
                            entryPoint,
                            contactedPeers,
                            distantJoinOptions,
                            retry);
                    }));
        }
        co_await folly::coro::collectAllRange(std::move(joins));
    }

    folly::coro::Task<void> joinThroughEntryPoint(
        PeerDescriptor entryPointDescriptor,
        std::set<DhtAddress>& contactedPeers,
        DistantJoinConfig& additionalDistantJoin,
        bool retry = true) {
        if (this->isStopped()) {
            co_return;
        }
        {
            std::scoped_lock lock(this->mutex);
            this->joinCalled = true;
        }
        SLogger::debug(
            "Joining DHT via entrypoint " +
            Identifiers::getNodeIdFromPeerDescriptor(entryPointDescriptor));
        if (Identifiers::areEqualPeerDescriptors(
                entryPointDescriptor, this->options.localPeerDescriptor)) {
            co_return;
        }
        if (this->options.connectionLocker != nullptr) {
            this->options.connectionLocker->lockConnection(
                entryPointDescriptor, this->joinLockId());
        }
        this->options.peerManager.addContact(entryPointDescriptor);
        const auto targetId = Identifiers::getNodeIdFromPeerDescriptor(
            this->options.localPeerDescriptor);
        std::vector<std::shared_ptr<DiscoverySession>> sessions;
        sessions.push_back(this->createSession(targetId, contactedPeers));
        if (additionalDistantJoin.enabled) {
            sessions.push_back(this->createSession(
                createDistantDhtAddress(targetId),
                additionalDistantJoin.contactedPeers));
        }
        co_await this->runSessions(
            std::move(sessions), entryPointDescriptor, retry);
        if (this->options.connectionLocker != nullptr) {
            this->options.connectionLocker->unlockConnection(
                entryPointDescriptor, this->joinLockId());
        }
    }

    folly::coro::Task<void> joinRing() {
        std::set<DhtAddress> contactedPeers;
        std::vector<std::shared_ptr<RingDiscoverySession>> sessions;
        sessions.push_back(this->createRingSession(
            getRingIdRawFromPeerDescriptor(this->options.localPeerDescriptor),
            contactedPeers));
        co_await this->runRingSessions(std::move(sessions));
    }

    folly::coro::Task<void> rejoinDht(
        PeerDescriptor entryPoint,
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        std::set<DhtAddress> contactedPeers = {},
        std::set<DhtAddress> distantJoinContactPeers = {}) {
        {
            std::scoped_lock lock(this->mutex);
            if (this->isStopped() || this->rejoinOngoing) {
                co_return;
            }
            this->rejoinOngoing = true;
        }
        SLogger::debug(
            "Rejoining DHT " +
            static_cast<std::string>(this->options.serviceId));
        DistantJoinConfig additionalDistantJoin{
            .enabled = true,
            .contactedPeers = std::move(distantJoinContactPeers)};
        try {
            co_await this->joinThroughEntryPoint(
                entryPoint, contactedPeers, additionalDistantJoin, true);
            SLogger::debug(
                "Rejoined DHT successfully " +
                static_cast<std::string>(this->options.serviceId));
        } catch (const std::exception&) {
            SLogger::warn(
                "Rejoining DHT failed " +
                static_cast<std::string>(this->options.serviceId));
            if (!this->isStopped()) {
                this->scheduleRejoin(entryPoint, rejoinRetryDelay);
            }
        }
        {
            std::scoped_lock lock(this->mutex);
            this->rejoinOngoing = false;
        }
    }

    [[nodiscard]] bool isJoinOngoing() {
        std::scoped_lock lock(this->mutex);
        return !this->joinCalled ? true
                                 : !this->ongoingDiscoverySessions.empty();
    }

    [[nodiscard]] bool isJoinCalled() {
        std::scoped_lock lock(this->mutex);
        return this->joinCalled;
    }
};

} // namespace streamr::dht::discovery
