#ifndef STREAMR_TRACKERLESS_NETWORK_PROPAGATION_HPP
#define STREAMR_TRACKERLESS_NETWORK_PROPAGATION_HPP

#include "PropagationTaskStore.hpp"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"

namespace streamr::trackerlessnetwork::propagation {

using streamr::dht::DhtAddress;
using SendToNeighborFn =
    std::function<void(const DhtAddress&, const StreamMessage&)>;

struct PropagationOptions {
    SendToNeighborFn sendToNeighbor;
    size_t minPropagationTargets;
    std::optional<std::chrono::milliseconds> ttl;
    std::optional<size_t> maxMessages;
};

constexpr size_t DEFAULT_MAX_MESSAGES = 150; // NOLINT
// NOLINTNEXTLINE
constexpr std::chrono::milliseconds DEFAULT_TTL = std::chrono::seconds(10);

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
    size_t minPropagationTargets;
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
    size_t feedUnseenMessage(
        const StreamMessage& message,
        const std::vector<DhtAddress>& targets,
        const std::optional<DhtAddress>& source) {
        PropagationTask task{
            .message = message,
            .source = source,
            .handledNeighbors = std::set<DhtAddress>()};

        this->activeTaskStore.add(task);
        size_t sentTo = 0;
        for (const auto& target : targets) {
            if (this->sendAndAwaitThenMark(task, target)) {
                sentTo++;
            }
        }
        return sentTo;
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

#endif // STREAMR_TRACKERLESS_NETWORK_PROPAGATION_HPP