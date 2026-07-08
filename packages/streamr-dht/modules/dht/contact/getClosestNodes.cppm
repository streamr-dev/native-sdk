// Module streamr.dht.getClosestNodes
// Ported from packages/dht/src/dht/contact/getClosestNodes.ts
// (v103.8.0-rc.3).
module;

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <vector>

export module streamr.dht.getClosestNodes;

import streamr.dht.protos;

import streamr.dht.Contact;
import streamr.dht.Identifiers;
import streamr.dht.SortedContactList;

export namespace streamr::dht::contact {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;

struct GetClosestNodesOptions {
    std::optional<size_t> maxCount;
    std::optional<std::set<DhtAddress>> excludedNodeIds;
};

inline std::vector<PeerDescriptor> getClosestNodes(
    const DhtAddress& referenceId,
    const std::vector<PeerDescriptor>& contacts,
    const GetClosestNodesOptions& opts = {}) {
    SortedContactList<Contact> list(
        SortedContactListOptions{
            .referenceId = referenceId,
            .allowToContainReferenceId = true,
            .maxSize = opts.maxCount,
            .nodeIdDistanceLimit = std::nullopt,
            .excludedNodeIds = opts.excludedNodeIds});
    for (const auto& contact : contacts) {
        list.addContact(std::make_shared<Contact>(contact));
    }
    std::vector<PeerDescriptor> result;
    for (const auto& contact : list.getClosestContacts()) {
        result.push_back(contact->getPeerDescriptor());
    }
    return result;
}

} // namespace streamr::dht::contact
