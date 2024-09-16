#ifndef STREAMR_TRACKERLESS_NETWORK_NODE_LIST_HPP
#define STREAMR_TRACKERLESS_NETWORK_NODE_LIST_HPP

#include <map>
#include <ranges>
#include <vector>
#include "streamr-dht/Identifiers.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-trackerless-network/logic/ContentDeliveryRpcRemote.hpp"

namespace streamr::trackerlessnetwork {

using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

class ContentDeliveryRpcRemote;
struct NodeAdded
    : Event<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>> {};
struct NodeRemoved
    : Event<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>> {};

using NodeListEvents = std::tuple<NodeAdded, NodeRemoved>;

// The items in the list are in the insertion order

class NodeList : public EventEmitter<NodeListEvents> {
private:
    std::map<DhtAddress, std::shared_ptr<ContentDeliveryRpcRemote>> nodes;
    size_t limit;
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
    NodeList(DhtAddress ownId, size_t limit)
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

    [[nodiscard]] std::optional<std::shared_ptr<ContentDeliveryRpcRemote>> get(
        const DhtAddress& id) const {
        return this->nodes.at(id);
    }

    [[nodiscard]] size_t size(
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
        return values[rand() % values.size()];
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
            this->remove(Identifiers::getNodeIdFromPeerDescriptor(
                node->getPeerDescriptor()));
        }
        this->removeAllListeners();
    }
};

} // namespace streamr::trackerlessnetwork

#endif // STREAMR_TRACKERLESS_NETWORK_NODE_LIST_HPP
