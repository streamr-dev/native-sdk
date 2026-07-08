// Module streamr.dht.DiscoverySession
// Ported from packages/dht/src/dht/discovery/DiscoverySession.ts
// (v103.8.0-rc.3). One Kademlia-style lookup toward a target id: it keeps
// up to `parallelism` getClosestPeers requests in flight against the closest
// uncontacted nearby contacts, feeds the returned peers back into the
// PeerManager, and finishes once the frontier is exhausted or `noProgressLimit`
// consecutive requests fail to bring a closer neighbour.
//
// TS drives the fan-out through promise continuations: each request's
// `.finally` re-invokes findMoreContacts, which tops the in-flight set back up
// to `parallelism`. This port expresses the identical rolling concurrency as
// `parallelism` worker coroutines that each pull the next-closest uncontacted
// node from the shared frontier under the session mutex, so at most
// `parallelism` requests run at once and each completion immediately refills.
// The TS Gate/withTimeout pair becomes structured concurrency: findClosestNodes
// awaits every worker under a single folly timeout (a `done` flag, guarded by
// the mutex, stands in for the single-shot Gate). Because the workers are
// awaited rather than detached, `this` outlives them and no self-pin is needed.
//
// Discovery is I/O-bound (it spends its time awaiting RPC responses, not
// computing), so unlike the Router's routing-table work it is not offloaded to
// a dedicated worker executor; the fan-out runs on whatever executor drives the
// join and yields at every RPC await.
module;
#include <new>



export module streamr.dht.DiscoverySession;

import std;

import streamr.dht.protos;

import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.Uuid;
import streamr.logger.SLogger;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.getClosestNodes;
import streamr.dht.getPeerDistance;
import streamr.dht.Identifiers;
import streamr.dht.PeerManager;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::AbortSignal;
using streamr::utils::Uuid;

export namespace streamr::dht::discovery {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::DhtNodeRpcRemote;
using streamr::dht::Identifiers;
using streamr::dht::PeerManager;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::contact::GetClosestNodesOptions;
using streamr::dht::helpers::getPeerDistance;

struct DiscoverySessionOptions {
    DhtAddress targetId;
    std::size_t parallelism;
    std::size_t noProgressLimit;
    PeerManager& peerManager;
    // Mutated by this session (and, when several entry points are joined at
    // once, by the sibling sessions that share the same set). Owned by the
    // caller; it outlives findClosestNodes.
    std::set<DhtAddress>& contactedPeers;
    AbortSignal& abortSignal;
    std::function<std::shared_ptr<DhtNodeRpcRemote>(const PeerDescriptor&)>
        createDhtNodeRpcRemote;
};

class DiscoverySession {
private:
    std::string id = Uuid::v4();
    std::size_t noProgressCounter = 0;
    std::set<DhtAddress> ongoingRequests;
    bool done = false; // the single-shot Gate, guarded by the mutex
    std::recursive_mutex mutex;
    DiscoverySessionOptions options;

    void addContacts(const std::vector<PeerDescriptor>& contacts) {
        std::scoped_lock lock(this->mutex);
        if (this->options.abortSignal.aborted || this->done) {
            return;
        }
        for (const auto& contact : contacts) {
            this->options.peerManager.addContact(contact);
        }
    }

    folly::coro::Task<std::vector<PeerDescriptor>>
    fetchClosestNeighborsFromRemote(PeerDescriptor peerDescriptor) {
        {
            std::scoped_lock lock(this->mutex);
            if (this->options.abortSignal.aborted || this->done) {
                co_return std::vector<PeerDescriptor>{};
            }
        }
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        SLogger::trace("Getting closest neighbors from remote: " + nodeId);
        const auto remote =
            this->options.createDhtNodeRpcRemote(peerDescriptor);
        auto returnedContacts =
            co_await remote->getClosestPeers(this->options.targetId);
        {
            std::scoped_lock lock(this->mutex);
            this->options.peerManager.setContactActive(nodeId);
        }
        co_return returnedContacts;
    }

    // Returns nullopt when there are no neighbours yet (TS would index [0] of
    // an empty array; guarding avoids the undefined access).
    [[nodiscard]] std::optional<PeerDescriptor> getClosestNeighbor() {
        std::vector<PeerDescriptor> neighborDescriptors;
        const auto neighbors = this->options.peerManager.getNeighbors();
        neighborDescriptors.reserve(neighbors.size());
        for (const auto& neighbor : neighbors) {
            neighborDescriptors.push_back(neighbor->getPeerDescriptor());
        }
        const auto closest = getClosestNodes(
            this->options.targetId,
            neighborDescriptors,
            GetClosestNodesOptions{.maxCount = 1});
        if (closest.empty()) {
            return std::nullopt;
        }
        return closest.front();
    }

    void onRequestSucceeded(
        const DhtAddress& nodeId, const std::vector<PeerDescriptor>& contacts) {
        std::scoped_lock lock(this->mutex);
        if (!this->ongoingRequests.contains(nodeId)) {
            return;
        }
        this->ongoingRequests.erase(nodeId);
        const auto targetIdRaw =
            Identifiers::getRawFromDhtAddress(this->options.targetId);
        const auto oldClosestNeighbor = this->getClosestNeighbor();
        this->addContacts(contacts);
        const auto newClosestNeighbor = this->getClosestNeighbor();
        // No prior neighbour means the newly added contacts are progress; a
        // new closest that is not nearer than the old one is not.
        if (oldClosestNeighbor.has_value() && newClosestNeighbor.has_value()) {
            const auto oldClosestDistance = getPeerDistance(
                targetIdRaw, DhtAddressRaw{oldClosestNeighbor->nodeid()});
            const auto newClosestDistance = getPeerDistance(
                targetIdRaw, DhtAddressRaw{newClosestNeighbor->nodeid()});
            if (newClosestDistance >= oldClosestDistance) {
                this->noProgressCounter++;
            }
        } else if (!newClosestNeighbor.has_value()) {
            this->noProgressCounter++;
        }
    }

    void onRequestFailed(const DhtAddress& nodeId) {
        std::scoped_lock lock(this->mutex);
        if (!this->ongoingRequests.contains(nodeId)) {
            return;
        }
        this->ongoingRequests.erase(nodeId);
        this->options.peerManager.removeContact(nodeId);
    }

    // One worker of the rolling fan-out: repeatedly claim the closest
    // uncontacted node and query it, until the session is done.
    folly::coro::Task<void> worker() {
        while (true) {
            std::optional<PeerDescriptor> node;
            {
                std::scoped_lock lock(this->mutex);
                if (this->options.abortSignal.aborted || this->done) {
                    co_return;
                }
                if (this->noProgressCounter >= this->options.noProgressLimit) {
                    this->done = true;
                    co_return;
                }
                const auto uncontacted = getClosestNodes(
                    this->options.targetId,
                    this->options.peerManager.getNearbyContacts(),
                    GetClosestNodesOptions{
                        .maxCount = this->options.parallelism,
                        .excludedNodeIds = this->options.contactedPeers});
                for (const auto& candidate : uncontacted) {
                    const auto candidateId =
                        Identifiers::getNodeIdFromPeerDescriptor(candidate);
                    if (!this->ongoingRequests.contains(candidateId)) {
                        node = candidate;
                        break;
                    }
                }
                if (!node.has_value()) {
                    if (this->ongoingRequests.empty()) {
                        this->done = true;
                    }
                    co_return;
                }
                const auto nodeId =
                    Identifiers::getNodeIdFromPeerDescriptor(*node);
                this->ongoingRequests.insert(nodeId);
                // TS adds to contactedPeers synchronously at the start of
                // fetchClosestNeighborsFromRemote, before the await; claiming
                // it here (under the lock) keeps sibling workers from picking
                // the same node.
                this->options.contactedPeers.insert(nodeId);
            }
            const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(*node);
            try {
                const auto contacts =
                    co_await this->fetchClosestNeighborsFromRemote(*node);
                this->onRequestSucceeded(nodeId, contacts);
            } catch (const std::exception& e) {
                SLogger::trace(
                    "getClosestPeers failed: " + std::string(e.what()));
                this->onRequestFailed(nodeId);
            }
        }
    }

public:
    explicit DiscoverySession(DiscoverySessionOptions options)
        : options(std::move(options)) {}

    [[nodiscard]] const std::string& getId() const { return this->id; }

    folly::coro::Task<void> findClosestNodes(
        std::chrono::milliseconds timeout) {
        {
            std::scoped_lock lock(this->mutex);
            if (this->options.peerManager.getNearbyContactCount(
                    this->options.contactedPeers) == 0) {
                co_return;
            }
        }
        std::vector<folly::coro::Task<void>> workers;
        workers.reserve(this->options.parallelism);
        for (std::size_t i = 0; i < this->options.parallelism; ++i) {
            workers.push_back(this->worker());
        }
        co_await streamr::utils::co_withCancellation(
            this->options.abortSignal.getCancellationToken(),
            folly::coro::timeout(
                folly::coro::collectAllRange(std::move(workers)), timeout));
    }
};

} // namespace streamr::dht::discovery
