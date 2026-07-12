// Module streamr.trackerlessnetwork.NodeList
// CONSOLIDATED from the former header logic/NodeList.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
//
// Storage is INSERTION-ORDERED (phase C3): the TS NodeList is a Map,
// whose iteration order is insertion order, and the protocol depends on
// it — getLast() must return the most recently added node (the handshake
// interleave hands the newest neighbor over), not the id-largest one as
// the earlier std::map-backed port did. Lookups are linear; the lists
// are bounded by small limits (default 20).
//
// Thread safety (phase C3): TS runs single-threaded; here RPC handler
// threads, the neighbor-finder chains, the neighbor-update interval and
// discovery-event handlers all mutate the same lists concurrently (a
// 64-node scale test crashed reading entries freed by a concurrent
// replaceAll). All container access is mutex-guarded; NodeAdded /
// NodeRemoved are emitted OUTSIDE the lock because their handlers make
// blocking RPC sends.
module;

#include <algorithm>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <vector>

export module streamr.trackerlessnetwork.NodeList;

import streamr.dht.Identifiers;
import streamr.eventemitter.EventEmitter;
import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
export namespace streamr::trackerlessnetwork {

// NOTE: the former header forward-declared ContentDeliveryRpcRemote
// here; in the named-sub-module world a purview forward declaration
// would attach the name to THIS module and conflict with the defining
// module, so the import above provides the declaration instead.
struct NodeAdded
    : Event<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>> {};
struct NodeRemoved
    : Event<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>> {};

using NodeListEvents = std::tuple<NodeAdded, NodeRemoved>;

class NodeList : public EventEmitter<NodeListEvents> {
private:
    using Entry =
        std::pair<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>>;

    mutable std::mutex mutex;
    std::vector<Entry> nodes;
    size_t limit;
    DhtAddress ownId;

    [[nodiscard]] std::vector<Entry>::const_iterator find(
        const DhtAddress& nodeId) const {
        return std::ranges::find_if(this->nodes, [&nodeId](const auto& entry) {
            return entry.first == nodeId;
        });
    }

    [[nodiscard]] std::vector<Entry>::iterator find(const DhtAddress& nodeId) {
        return std::ranges::find_if(this->nodes, [&nodeId](const auto& entry) {
            return entry.first == nodeId;
        });
    }

    static std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>
    getValuesOfIncludedKeys(
        const std::vector<Entry>& nodes,
        const std::vector<DhtAddress>& exclude,
        bool wsOnly = false) {
        return nodes | std::views::values |
            std::views::filter([&exclude, &wsOnly](const auto& node) {
                   if (wsOnly && !node->getPeerDescriptor().has_websocket()) {
                       return false;
                   }
                   return std::find(
                              exclude.begin(),
                              exclude.end(),
                              Identifiers::getNodeIdFromPeerDescriptor(
                                  node->getPeerDescriptor())) == exclude.end();
               }) |
            std::ranges::to<
                   std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>>();
    }

public:
    NodeList(DhtAddress ownId, size_t limit)
        : limit(limit), ownId(std::move(ownId)) {}

    void add(const std::shared_ptr<ContentDeliveryRpcRemote>& remote) {
        const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
            remote->getPeerDescriptor());
        bool added = false;
        {
            std::scoped_lock lock(this->mutex);
            if ((this->ownId != nodeId) && (this->nodes.size() < this->limit)) {
                const auto it = this->find(nodeId);
                if (it != this->nodes.end()) {
                    it->second = remote;
                } else {
                    this->nodes.emplace_back(nodeId, remote);
                    added = true;
                }
            }
        }
        if (added) {
            this->emit<NodeAdded>(nodeId, remote);
        }
    }

    void remove(const DhtAddress& nodeId) {
        std::shared_ptr<ContentDeliveryRpcRemote> removed;
        {
            std::scoped_lock lock(this->mutex);
            const auto it = this->find(nodeId);
            if (it != this->nodes.end()) {
                removed = it->second;
                this->nodes.erase(it);
            }
        }
        if (removed) {
            this->emit<NodeRemoved>(nodeId, removed);
        }
    }

    [[nodiscard]] bool has(const DhtAddress& nodeId) const {
        std::scoped_lock lock(this->mutex);
        return this->find(nodeId) != this->nodes.end();
    }

    // Replace nodes does not emit nodeRemoved events, use with caution
    void replaceAll(
        const std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>&
            neighbors) {
        std::vector<Entry> addedEntries;
        {
            std::scoped_lock lock(this->mutex);
            this->nodes.clear();
            for (const auto& remote :
                 neighbors | std::views::take(this->limit)) {
                const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
                    remote->getPeerDescriptor());
                if ((this->ownId != nodeId) &&
                    (this->nodes.size() < this->limit) &&
                    this->find(nodeId) == this->nodes.end()) {
                    this->nodes.emplace_back(nodeId, remote);
                    addedEntries.emplace_back(nodeId, remote);
                }
            }
        }
        for (const auto& [nodeId, remote] : addedEntries) {
            this->emit<NodeAdded>(nodeId, remote);
        }
    }

    [[nodiscard]] std::vector<DhtAddress> getIds() const {
        std::scoped_lock lock(this->mutex);
        return this->nodes | std::views::keys |
            std::ranges::to<std::vector<DhtAddress>>();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getPeerDescriptors() const {
        std::scoped_lock lock(this->mutex);
        return this->nodes | std::views::values |
            std::views::transform([](const auto& remote) {
                   return remote->getPeerDescriptor();
               }) |
            std::ranges::to<std::vector<PeerDescriptor>>();
    }

    // TS getNode() returns undefined for unknown ids.
    [[nodiscard]] std::optional<std::shared_ptr<ContentDeliveryRpcRemote>> get(
        const DhtAddress& id) const {
        std::scoped_lock lock(this->mutex);
        if (const auto it = this->find(id); it != this->nodes.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    [[nodiscard]] size_t size(
        const std::vector<DhtAddress>& exclude = {}) const {
        std::scoped_lock lock(this->mutex);
        return NodeList::getValuesOfIncludedKeys(this->nodes, exclude).size();
    }

    [[nodiscard]] std::optional<std::shared_ptr<ContentDeliveryRpcRemote>>
    getRandom(const std::vector<DhtAddress>& exclude) {
        std::scoped_lock lock(this->mutex);
        const auto values =
            NodeList::getValuesOfIncludedKeys(this->nodes, exclude);
        if (values.empty()) {
            return std::nullopt;
        }
        return values[rand() % values.size()];
    }

    [[nodiscard]] std::optional<std::shared_ptr<ContentDeliveryRpcRemote>>
    getFirst(const std::vector<DhtAddress>& exclude, bool wsOnly = false) {
        std::scoped_lock lock(this->mutex);
        const auto included =
            NodeList::getValuesOfIncludedKeys(this->nodes, exclude, wsOnly);
        return included.empty() ? std::nullopt
                                : std::make_optional(included[0]);
    }

    [[nodiscard]] std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>
    getFirstAndLast(const std::vector<DhtAddress>& exclude) {
        std::scoped_lock lock(this->mutex);
        const auto included =
            NodeList::getValuesOfIncludedKeys(this->nodes, exclude);
        if (included.empty()) {
            return {};
        }
        if (included.size() > 1) {
            return {included.front(), included.back()};
        }
        return {included.front()};
    }

    // TS getLast(exclude): the insertion-order last node not excluded.
    [[nodiscard]] std::optional<std::shared_ptr<ContentDeliveryRpcRemote>>
    getLast(const std::vector<DhtAddress>& exclude) const {
        std::scoped_lock lock(this->mutex);
        const auto included =
            NodeList::getValuesOfIncludedKeys(this->nodes, exclude);
        return included.empty() ? std::nullopt
                                : std::make_optional(included.back());
    }

    [[nodiscard]] std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>
    getAll() {
        std::scoped_lock lock(this->mutex);
        return this->nodes | std::views::values |
            std::ranges::to<
                   std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>>();
    }

    void stop() {
        for (const auto& node : this->getAll()) {
            this->remove(
                Identifiers::getNodeIdFromPeerDescriptor(
                    node->getPeerDescriptor()));
        }
        this->removeAllListeners();
    }
};

} // namespace streamr::trackerlessnetwork
