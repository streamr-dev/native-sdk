#ifndef STREAMR_TRACKERLESS_NETWORK_PROPAGATIONTASKSTORE_HPP
#define STREAMR_TRACKERLESS_NETWORK_PROPAGATIONTASKSTORE_HPP

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include "FifoMapWithTTL.hpp"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"

namespace streamr::trackerlessnetwork::propagation {

using streamr::dht::DhtAddress;

struct PropagationTask {
    StreamMessage message;
    std::optional<std::string> source;
    std::set<DhtAddress> handledNeighbors;
};

class PropagationTaskStore {
private:
    FifoMapWithTTL<MessageRef, PropagationTask> tasks;

public:
    PropagationTaskStore(std::chrono::milliseconds ttl, size_t maxTasks)
        : tasks(FifoMapWithTtlOptions<MessageRef>{
              .ttl = ttl,
              .maxSize = maxTasks,
          }) {}

    std::vector<PropagationTask> get() { return this->tasks.values(); }

    void add(const PropagationTask& task) {
        const auto messageId = task.message.messageid();
        MessageRef messageRef;
        this->tasks.set(messageIdToMessageRef(messageId), task);
    }

    void remove(const MessageRef& messageId) { this->tasks.remove(messageId); }

    static MessageRef messageIdToMessageRef(const MessageID& messageId) {
        MessageRef messageRef;
        messageRef.set_sequencenumber(messageId.sequencenumber());
        messageRef.set_timestamp(messageId.timestamp());
        return messageRef;
    }
};

} // namespace streamr::trackerlessnetwork::propagation

#endif