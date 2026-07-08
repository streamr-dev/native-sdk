// Module streamr.trackerlessnetwork.Propagation
// CONSOLIDATED from the former header logic/propagation/Propagation.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.Propagation;

import std;

import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.trackerlessnetwork.PropagationTaskStore;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
export namespace streamr::trackerlessnetwork::propagation {

using ::dht::PeerDescriptor;
using SendToNeighborFn =
    std::function<void(const DhtAddress&, const StreamMessage&)>;

struct PropagationOptions {
    SendToNeighborFn sendToNeighbor;
    std::size_t minPropagationTargets;
    std::optional<std::chrono::milliseconds> ttl;
    std::optional<std::size_t> maxMessages;
};

inline constexpr std::size_t DEFAULT_MAX_MESSAGES = 150; // NOLINT
// NOLINTNEXTLINE
inline constexpr std::chrono::milliseconds DEFAULT_TTL =
    std::chrono::seconds(10);

/**
 * Message propagation logic of a node. Given a message, this class will
 * actively attempt to propagate it to `minPropagationTargets` neighbors until
 * success or TTL expiration.
 *
 * Setting `minPropagationTargets = 0` effectively disables any propagation
 * reattempts. A message will then only be propagated exactly once, to neighbors
 * that are present at that moment, in a fire-and-forget manner.
 */

class Propagation {
private:
    SendToNeighborFn sendToNeighbor;
    std::size_t minPropagationTargets;
    PropagationTaskStore activeTaskStore;

public:
    explicit Propagation(const PropagationOptions& options)
        : activeTaskStore(
              options.ttl.value_or(DEFAULT_TTL),
              options.maxMessages.value_or(DEFAULT_MAX_MESSAGES)) {
        this->sendToNeighbor = options.sendToNeighbor;
        this->minPropagationTargets = options.minPropagationTargets;
    }

    /**
     * Node should invoke this when it learns about a new message
     * @return number of neighbors the message was sent to immediately
     */
    std::pair<
        std::vector<PeerDescriptor> /* failed to send to */,
        std::vector<PeerDescriptor> /* successfully sent to */>
    feedUnseenMessage(
        const StreamMessage& message,
        const std::vector<PeerDescriptor>& targets,
        const std::optional<DhtAddress>& source) {
        PropagationTask task{
            .message = message,
            .source = source,
            .handledNeighbors = std::set<DhtAddress>()};

        this->activeTaskStore.add(task);
        std::vector<PeerDescriptor> succesfulSends;
        std::vector<PeerDescriptor> failedSends;
        for (const auto& target : targets) {
            const auto neighborId =
                Identifiers::getNodeIdFromPeerDescriptor(target);
            if (this->sendAndAwaitThenMark(task, neighborId)) {
                succesfulSends.push_back(target);
            } else {
                failedSends.push_back(target);
            }
        }
        return {failedSends, succesfulSends};
    }

    /**
     * Node should invoke this when it learns about a new node stream assignment
     */
    void onNeighborJoined(const DhtAddress& neighborId) {
        const auto tasks = this->activeTaskStore.get();
        for (auto task : tasks) {
            this->sendAndAwaitThenMark(task, neighborId);
        }
    }

private:
    bool sendAndAwaitThenMark(
        PropagationTask& task, const DhtAddress& neighborId) {
        if (!task.handledNeighbors.contains(neighborId) &&
            neighborId != task.source) {
            try {
                this->sendToNeighbor(neighborId, task.message);
            } catch (...) {
                return false;
            }
            // Side-note: due to asynchronicity, the task being modified at
            // this point could already be stale and deleted from
            // `activeTaskStore`. However, as modifying it or re-deleting it
            // is pretty much inconsequential at this point, leaving the
            // logic as is.
            task.handledNeighbors.insert(neighborId);
            if (task.handledNeighbors.size() >= this->minPropagationTargets) {
                this->activeTaskStore.remove(
                    PropagationTaskStore::messageIdToMessageRef(
                        task.message.messageid()));
            }
            return true;
        }
        return false;
    }
};

} // namespace streamr::trackerlessnetwork::propagation
