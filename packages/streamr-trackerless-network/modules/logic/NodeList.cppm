// Module streamr.trackerlessnetwork.NodeList
// CONSOLIDATED from the former header logic/NodeList.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;
#include <new> // operator new ambiguity under import std (local-type container allocation) — see convert-to-import-std.py


export module streamr.trackerlessnetwork.NodeList;

import std;

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

// The items in the list are in the insertion order

class NodeList : public EventEmitter<NodeListEvents> {
private:
    std::map<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>> nodes;
    std::size_t limit;
    DhtAddress ownId;

    static std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>
    getValuesOfIncludedKeys(
        const std::map<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>>&
            nodes,
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
    NodeList(DhtAddress ownId, std::size_t limit)
        : limit(limit), ownId(std::move(ownId)) {}

    void add(const std::shared_ptr<ContentDeliveryRpcRemote>& remote) {
        const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
            remote->getPeerDescriptor());
        if ((this->ownId != nodeId) && (this->nodes.size() < this->limit)) {
            const auto isExistingNode = this->nodes.contains(nodeId);
            this->nodes[nodeId] = remote;

            if (!isExistingNode) {
                this->emit<NodeAdded>(nodeId, remote);
            }
        }
    }

    void remove(const DhtAddress& nodeId) {
        if (this->nodes.contains(nodeId)) {
            const auto remote = this->nodes.at(nodeId);
            this->nodes.erase(nodeId);
            this->emit<NodeRemoved>(nodeId, remote);
        }
    }

    [[nodiscard]] bool has(const DhtAddress& nodeId) const {
        return this->nodes.contains(nodeId);
    }

    // Replace nodes does not emit nodeRemoved events, use with caution
    void replaceAll(
        const std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>&
            neighbors) {
        this->nodes.clear();
        const auto limited =
            neighbors | std::views::take(this->limit) |
            std::ranges::to<
                std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>>();
        for (const auto& remote : limited) {
            this->add(remote);
        }
    }

    [[nodiscard]] std::vector<DhtAddress> getIds() const {
        return this->nodes | std::views::keys |
            std::ranges::to<std::vector<DhtAddress>>();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getPeerDescriptors() const {
        return this->nodes | std::views::values |
            std::views::transform([](const auto& remote) {
                   return remote->getPeerDescriptor();
               }) |
            std::ranges::to<std::vector<PeerDescriptor>>();
    }

    [[nodiscard]] std::optional<std::shared_ptr<ContentDeliveryRpcRemote>> get(
        const DhtAddress& id) const {
        return this->nodes.at(id);
    }

    [[nodiscard]] std::size_t size(
        const std::vector<DhtAddress>& exclude = {}) const {
        return NodeList::getValuesOfIncludedKeys(this->nodes, exclude).size();
    }

    [[nodiscard]] std::optional<std::shared_ptr<ContentDeliveryRpcRemote>>
    getRandom(const std::vector<DhtAddress>& exclude) {
        const auto values =
            NodeList::getValuesOfIncludedKeys(this->nodes, exclude);
        if (values.empty()) {
            return std::nullopt;
        }
        return values[std::rand() % values.size()];
    }

    [[nodiscard]] std::optional<std::shared_ptr<ContentDeliveryRpcRemote>>
    getFirst(const std::vector<DhtAddress>& exclude, bool wsOnly = false) {
        const auto included =
            NodeList::getValuesOfIncludedKeys(this->nodes, exclude, wsOnly);
        return included.empty() ? std::nullopt
                                : std::make_optional(included[0]);
    }

    [[nodiscard]] std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>
    getFirstAndLast(const std::vector<DhtAddress>& exclude) {
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

    [[nodiscard]] static std::optional<
        std::shared_ptr<ContentDeliveryRpcRemote>>
    getLast(
        const std::map<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>>&
            nodes,
        const std::vector<DhtAddress>& exclude) {
        const auto included = NodeList::getValuesOfIncludedKeys(nodes, exclude);
        return included.empty() ? std::nullopt
                                : std::make_optional(included.back());
    }

    [[nodiscard]] std::vector<std::shared_ptr<ContentDeliveryRpcRemote>>
    getAll() {
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
