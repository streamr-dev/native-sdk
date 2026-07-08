// Module streamr.trackerlessnetwork.PropagationTaskStore
// CONSOLIDATED from the former header
// logic/propagation/PropagationTaskStore.hpp (MODERNIZATION.md Phase 2.6): this
// file is now the source of truth.
module;
#include <new>

#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.PropagationTaskStore;

import std;

import streamr.dht.Identifiers;
import streamr.trackerlessnetwork.FifoMapWithTTL;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::DhtAddress;
export namespace streamr::trackerlessnetwork::propagation {

struct PropagationTask {
    StreamMessage message;
    std::optional<std::string> source;
    std::set<DhtAddress> handledNeighbors;
};

class PropagationTaskStore {
private:
    FifoMapWithTTL<MessageRef, PropagationTask> tasks;

public:
    PropagationTaskStore(std::chrono::milliseconds ttl, std::size_t maxTasks)
        : tasks(
              FifoMapWithTtlOptions<MessageRef>{
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
