#ifndef STREAMR_TRACKERLESS_NETWORK_NODE_LIST_HPP
#define STREAMR_TRACKERLESS_NETWORK_NODE_LIST_HPP

#include <map>
#include <string>
#include <vector>
#include <ranges>
#include "streamr-dht/Identifiers.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"

namespace streamr::trackerlessnetwork {

using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

class ContentDeliveryRpcRemote;
struct NodeAdded : Event<DhtAddress, ContentDeliveryRpcRemote> {};
struct NodeRemoved : Event<DhtAddress, ContentDeliveryRpcRemote> {};

using NodeListEvents = std::tuple<NodeAdded, NodeRemoved>;

// The items in the list are in the insertion order

class NodeList : public EventEmitter<NodeListEvents> {
private:
    std::map<DhtAddress, ContentDeliveryRpcRemote> nodes;
    size_t limit;
    DhtAddress ownId;

    static std::vector<ContentDeliveryRpcRemote> getValuesOfIncludedKeys(
        const std::map<DhtAddress, ContentDeliveryRpcRemote>& nodes,
        const std::vector<DhtAddress>& exclude, 
        bool wsOnly = false) {
        
        nodes 
        | std::views::values 
        | std::views::filter([&exclude, &wsOnly](const auto& node) {
            if (wsOnly && !node.getPeerDescriptor().websocket.has_value()) {
                return false;
            }
            return std::find(exclude.begin(), exclude.end(), 
                Identifiers::getNodeIdFromPeerDescriptor(node.getPeerDescriptor())) == exclude.end();
        })
        | std::ranges::to<std::vector>();
    }

public:
    NodeList(DhtAddress ownId, size_t limit) : limit(limit), ownId(ownId) {}

    void add(ContentDeliveryRpcRemote remote) {
        const nodeId =
            getNodeIdFromPeerDescriptor(remote.getPeerDescriptor()) if (
                (this.ownId != = nodeId) && (this.nodes.size < this.limit)) {
            const isExistingNode =
                this.nodes.has(nodeId) this.nodes.set(nodeId, remote)

                    if (!isExistingNode) {
                this.emit('nodeAdded', nodeId, remote)
            }
        }
    }

    void remove(DhtAddress nodeId) {
        if (this->nodes.contains(nodeId)) {
            const auto remote = this->nodes.at(nodeId);
            this->nodes.erase(nodeId);
            this->emit<NodeRemoved>(nodeId, remote);
        }
    }

    bool has(DhtAddress nodeId) {
        return this->nodes.contains(nodeId);
    }

    // Replace nodes does not emit nodeRemoved events, use with caution
    void replaceAll(std::vector<ContentDeliveryRpcRemote> neighbors) {
        this->nodes.clear();
        const limited = neighbors | std::views::take(this->limit) | std::ranges::to<std::vector>();
        for (const auto& remote : limited) {
            this->add(remote);
        }
    }

    std::vector<DhtAddress> getIds() {
        return std::vector<DhtAddress>(this->nodes | std::views::keys | std::ranges::to<std::vector>());
    }

    std::optional<ContentDeliveryRpcRemote> get(DhtAddress id) {
        return this->nodes.at(id);
    }

    size_t size(std::vector<DhtAddress> exclude = {}) {
        return this->getIds().size();
    }

    std::optional<ContentDeliveryRpcRemote> getRandom(std::vector<DhtAddress> exclude) {
        return this->getValuesOfIncludedKeys(this->nodes, exclude);
    }

    std::optional<ContentDeliveryRpcRemote> getFirst(std::vector<DhtAddress> exclude, bool wsOnly = false) {
        const included = this->getValuesOfIncludedKeys(this->nodes, exclude, wsOnly);
        return included.empty() ? std::nullopt : std::make_optional(included[0]);
    }
    }

    std::vector<ContentDeliveryRpcRemote> getFirstAndLast(std::vector<DhtAddress> exclude) {
        const included = this->getValuesOfIncludedKeys(this->nodes, exclude);
        if (included.empty()) {
            return []
        }
        return included.size() > 1
            ? [ this->getFirst(exclude) !, this->getLast(exclude) !]
            : [this->getFirst(exclude) !]
    }

    std::optional<ContentDeliveryRpcRemote> getLast(std::vector<DhtAddress> exclude) {
        const included = this->getValuesOfIncludedKeys(this->nodes, exclude);
        return included.empty() ? std::nullopt : std::make_optional(included.back());
    }

    std::vector<ContentDeliveryRpcRemote> getAll() {
        return std::vector<ContentDeliveryRpcRemote>(this->nodes.values());
    }

    void stop() {
        for (const auto& node : this->nodes) {
            this->remove(node.first);
        }
        this->removeAllListeners();
    }
};

} // namespace streamr::trackerlessnetwork

#endif // STREAMR_TRACKERLESS_NETWORK_NODE_LIST_HPP
