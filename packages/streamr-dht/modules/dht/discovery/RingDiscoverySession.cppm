// Module streamr.dht.RingDiscoverySession
// Ported from packages/dht/src/dht/discovery/RingDiscoverySession.ts
// (v103.8.0-rc.3). The ring-topology counterpart of DiscoverySession: it walks
// toward a target ring id by querying getClosestRingPeers on the closest
// uncontacted ring contacts, asking both sides (left/right) of the ring
// equally, and stops when the ring frontier is exhausted or `noProgressLimit`
// consecutive requests bring no closer contact on either side.
//
// The concurrency model is identical to DiscoverySession's: `parallelism`
// worker coroutines pull from the shared ring frontier under the session mutex
// (the faithful realisation of TS's `.finally`-recursion fan-out), and
// findClosestNodes awaits them under one folly timeout with a `done` flag
// standing in for the TS Gate. See DiscoverySession for the rationale.
module;

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <coroutine> // IWYU pragma: keep

export module streamr.dht.RingDiscoverySession;

import streamr.dht.protos;

import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.Uuid;
import streamr.logger.SLogger;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.Identifiers;
import streamr.dht.PeerManager;
import streamr.dht.RingContactList;
import streamr.dht.ringIdentifiers;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::AbortSignal;
using streamr::utils::Uuid;

export namespace streamr::dht::discovery {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::DhtNodeRpcRemote;
using streamr::dht::Identifiers;
using streamr::dht::PeerManager;
using streamr::dht::contact::getLeftDistance;
using streamr::dht::contact::getRingIdFromPeerDescriptor;
using streamr::dht::contact::getRingIdFromRaw;
using streamr::dht::contact::RingContacts;
using streamr::dht::contact::RingId;
using streamr::dht::contact::RingIdRaw;

struct RingDiscoverySessionOptions {
    RingIdRaw targetId;
    size_t parallelism;
    size_t noProgressLimit;
    PeerManager& peerManager;
    // Mutated by this session (and sibling sessions that share the set); owned
    // by the caller and outlives findClosestNodes.
    std::set<DhtAddress>& contactedPeers;
    AbortSignal& abortSignal;
};

class RingDiscoverySession {
private:
    std::string id = Uuid::v4();
    size_t noProgressCounter = 0;
    std::set<DhtAddress> ongoingRequests;
    bool done = false; // the single-shot Gate, guarded by the mutex
    std::recursive_mutex mutex;
    RingDiscoverySessionOptions options;
    RingId targetIdAsRingId;

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
    fetchClosestContactsFromRemote(std::shared_ptr<DhtNodeRpcRemote> contact) {
        {
            std::scoped_lock lock(this->mutex);
            if (this->options.abortSignal.aborted || this->done) {
                co_return std::vector<PeerDescriptor>{};
            }
        }
        SLogger::trace(
            "Getting closest ring peers from contact: " + contact->getNodeId());
        auto returnedContacts =
            co_await contact->getClosestRingPeers(this->options.targetId);
        {
            std::scoped_lock lock(this->mutex);
            this->options.peerManager.setContactActive(contact->getNodeId());
        }
        // Merge the wire response's left/right peer lists into a single set of
        // contacts to feed back (TS concatenates them before addContacts).
        std::vector<PeerDescriptor> merged;
        merged.reserve(
            returnedContacts.left.size() + returnedContacts.right.size());
        merged.insert(
            merged.end(),
            returnedContacts.left.begin(),
            returnedContacts.left.end());
        merged.insert(
            merged.end(),
            returnedContacts.right.begin(),
            returnedContacts.right.end());
        co_return merged;
    }

    // The left/right distance of the closest current ring contact to the
    // target, or nullopt when that side has no contact (TS indexes [0] of a
    // possibly empty array; guarding avoids the undefined access).
    [[nodiscard]] std::optional<RingId> closestSideDistance(bool leftSide) {
        const auto closest = this->options.peerManager.getClosestRingContactsTo(
            this->options.targetId, 1);
        const auto& side = leftSide ? closest.left : closest.right;
        if (side.empty()) {
            return std::nullopt;
        }
        return getLeftDistance(
            this->targetIdAsRingId,
            getRingIdFromPeerDescriptor(side.front()->getPeerDescriptor()));
    }

    static bool sideMadeNoProgress(
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        std::optional<RingId> oldDistance,
        std::optional<RingId> newDistance) {
        if (oldDistance.has_value() && newDistance.has_value()) {
            return *newDistance >= *oldDistance;
        }
        // Gaining a contact where there was none is progress; anything else
        // (no contact on this side) counts as no progress.
        return !(!oldDistance.has_value() && newDistance.has_value());
    }

    void onRequestSucceeded(
        const DhtAddress& nodeId, const std::vector<PeerDescriptor>& contacts) {
        std::scoped_lock lock(this->mutex);
        if (!this->ongoingRequests.contains(nodeId)) {
            return;
        }
        this->ongoingRequests.erase(nodeId);
        const auto oldLeft = this->closestSideDistance(true);
        const auto oldRight = this->closestSideDistance(false);
        this->addContacts(contacts);
        const auto newLeft = this->closestSideDistance(true);
        const auto newRight = this->closestSideDistance(false);
        if (sideMadeNoProgress(oldLeft, newLeft) &&
            sideMadeNoProgress(oldRight, newRight)) {
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

    // Ask from both sides of the ring equally, dropping duplicates (TS
    // interleaves left[i]/right[i]).
    [[nodiscard]] static std::vector<std::shared_ptr<DhtNodeRpcRemote>>
    mergeSides(const RingContacts<DhtNodeRpcRemote>& uncontacted) {
        std::vector<std::shared_ptr<DhtNodeRpcRemote>> merged;
        std::set<DhtAddress> alreadyInMerged;
        const size_t length =
            std::max(uncontacted.left.size(), uncontacted.right.size());
        for (size_t i = 0; i < length; ++i) {
            if (i < uncontacted.left.size() &&
                alreadyInMerged.insert(uncontacted.left[i]->getNodeId())
                    .second) {
                merged.push_back(uncontacted.left[i]);
            }
            if (i < uncontacted.right.size() &&
                alreadyInMerged.insert(uncontacted.right[i]->getNodeId())
                    .second) {
                merged.push_back(uncontacted.right[i]);
            }
        }
        return merged;
    }

    folly::coro::Task<void> worker() {
        while (true) {
            std::shared_ptr<DhtNodeRpcRemote> contact;
            {
                std::scoped_lock lock(this->mutex);
                if (this->options.abortSignal.aborted || this->done) {
                    co_return;
                }
                if (this->noProgressCounter >= this->options.noProgressLimit) {
                    this->done = true;
                    co_return;
                }
                const auto uncontacted =
                    this->options.peerManager.getClosestRingContactsTo(
                        this->options.targetId,
                        this->options.parallelism,
                        this->options.contactedPeers);
                if (uncontacted.left.empty() && uncontacted.right.empty()) {
                    this->done = true;
                    co_return;
                }
                for (const auto& candidate : mergeSides(uncontacted)) {
                    if (!this->ongoingRequests.contains(
                            candidate->getNodeId())) {
                        contact = candidate;
                        break;
                    }
                }
                if (contact == nullptr) {
                    co_return;
                }
                const auto nodeId = contact->getNodeId();
                this->ongoingRequests.insert(nodeId);
                // TS adds to contactedPeers synchronously at the start of the
                // fetch; claiming it here keeps sibling workers off this node.
                this->options.contactedPeers.insert(nodeId);
            }
            const auto nodeId = contact->getNodeId();
            try {
                const auto contacts =
                    co_await this->fetchClosestContactsFromRemote(contact);
                this->onRequestSucceeded(nodeId, contacts);
            } catch (const std::exception& e) {
                SLogger::trace(
                    "getClosestRingPeers failed: " + std::string(e.what()));
                this->onRequestFailed(nodeId);
            }
        }
    }

public:
    explicit RingDiscoverySession(RingDiscoverySessionOptions options)
        : options(std::move(options)),
          targetIdAsRingId(getRingIdFromRaw(this->options.targetId)) {}

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
        for (size_t i = 0; i < this->options.parallelism; ++i) {
            workers.push_back(this->worker());
        }
        // MERGE the ambient cancellation with the abort signal (see
        // DiscoverySession::findClosestNodes for why replacing it hangs
        // the stop()-time scope drain).
        co_await streamr::utils::co_withCancellation(
            streamr::utils::cancellationTokenMerge(
                co_await streamr::utils::co_currentCancellationToken(),
                this->options.abortSignal.getCancellationToken()),
            folly::coro::timeout(
                folly::coro::collectAllRange(std::move(workers)), timeout));
    }
};

} // namespace streamr::dht::discovery
