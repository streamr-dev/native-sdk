// Module streamr.dht.RingContactList
// Ported from packages/dht/src/dht/contact/RingContactList.ts
// (v103.8.0-rc.3). The TS OrderedMap<RingDistance, C> is a std::map here
// (ascending key order, same neighbour semantics).
module;
#include <new>


export module streamr.dht.RingContactList;

import std;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.dht.ContactList;
import streamr.dht.Identifiers;
import streamr.dht.ringIdentifiers;

// Hoisted from the former header (file scope, NOT exported).
using streamr::eventemitter::EventEmitter;

export namespace streamr::dht::contact {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;

// The element type the ring contact list holds: anything that can report
// its PeerDescriptor. Replaces TS's `C extends { getPeerDescriptor():
// PeerDescriptor }`.
template <typename C>
concept HasGetPeerDescriptor = requires(const C& contact) {
    { contact.getPeerDescriptor() } -> std::convertible_to<PeerDescriptor>;
};

template <HasGetPeerDescriptor C>
struct RingContacts {
    std::vector<std::shared_ptr<C>> left;
    std::vector<std::shared_ptr<C>> right;
};

template <HasGetPeerDescriptor C>
class RingContactList : public EventEmitter<ContactListEvents<C>> {
private:
    static constexpr std::size_t numNeighborsPerSide = 5;
    RingId referenceId;
    std::set<DhtAddress> excludedIds;
    // Ascending by distance; the closest neighbours are at the front.
    std::map<RingDistance, std::shared_ptr<C>> leftNeighbors;
    std::map<RingDistance, std::shared_ptr<C>> rightNeighbors;

    // Keeps the closest numNeighborsPerSide by distance; returns whether it
    // added and/or evicted, so the caller can emit the right events.
    static void insertBounded(
        std::map<RingDistance, std::shared_ptr<C>>& neighbors,
        RingDistance distance,
        const std::shared_ptr<C>& contact,
        bool& elementAdded,
        bool& elementRemoved) {
        const bool empty = neighbors.empty();
        // std::prev(end()) is the largest (furthest) key.
        if (empty || distance < std::prev(neighbors.end())->first) {
            neighbors.insert_or_assign(distance, contact);
            elementAdded = true;
            if (neighbors.size() > numNeighborsPerSide) {
                neighbors.erase(std::prev(neighbors.end()));
                elementRemoved = true;
            }
        }
    }

public:
    explicit RingContactList(
        const RingIdRaw& rawReferenceId,
        std::optional<std::set<DhtAddress>> excludedIds = std::nullopt)
        : referenceId(getRingIdFromRaw(rawReferenceId)),
          excludedIds(excludedIds.value_or(std::set<DhtAddress>{})) {}

    void addContact(const std::shared_ptr<C>& contact) {
        const RingId id =
            getRingIdFromPeerDescriptor(contact->getPeerDescriptor());
        if (id == this->referenceId ||
            this->excludedIds.contains(
                Identifiers::getNodeIdFromPeerDescriptor(
                    contact->getPeerDescriptor()))) {
            return;
        }
        bool elementAdded = false;
        bool elementRemoved = false;

        insertBounded(
            this->leftNeighbors,
            getLeftDistance(this->referenceId, id),
            contact,
            elementAdded,
            elementRemoved);
        insertBounded(
            this->rightNeighbors,
            getRightDistance(this->referenceId, id),
            contact,
            elementAdded,
            elementRemoved);

        if (elementAdded) {
            this->template emit<contactlistevents::ContactAdded<C>>(contact);
        }
        if (elementRemoved) {
            this->template emit<contactlistevents::ContactRemoved<C>>(contact);
        }
    }

    void removeContact(const std::shared_ptr<C>& contact) {
        if (!contact) {
            return;
        }
        const RingId id =
            getRingIdFromPeerDescriptor(contact->getPeerDescriptor());
        const RingDistance leftDistance =
            getLeftDistance(this->referenceId, id);
        const RingDistance rightDistance =
            getRightDistance(this->referenceId, id);

        bool elementRemoved = false;
        if (this->leftNeighbors.erase(leftDistance) > 0) {
            elementRemoved = true;
        }
        if (this->rightNeighbors.erase(rightDistance) > 0) {
            elementRemoved = true;
        }
        if (elementRemoved) {
            this->template emit<contactlistevents::ContactRemoved<C>>(contact);
        }
    }

    [[nodiscard]] std::shared_ptr<C> getContact(
        const PeerDescriptor& peerDescriptor) const {
        const RingId id = getRingIdFromPeerDescriptor(peerDescriptor);
        const auto leftIt =
            this->leftNeighbors.find(getLeftDistance(this->referenceId, id));
        if (leftIt != this->leftNeighbors.end()) {
            return leftIt->second;
        }
        const auto rightIt =
            this->rightNeighbors.find(getRightDistance(this->referenceId, id));
        if (rightIt != this->rightNeighbors.end()) {
            return rightIt->second;
        }
        return nullptr;
    }

    [[nodiscard]] RingContacts<C> getClosestContacts(
        std::optional<std::size_t> limitPerSide = std::nullopt) const {
        RingContacts<C> result;
        for (const auto& [distance, contact] : this->leftNeighbors) {
            if (limitPerSide.has_value() &&
                result.left.size() >= limitPerSide.value()) {
                break;
            }
            result.left.push_back(contact);
        }
        for (const auto& [distance, contact] : this->rightNeighbors) {
            if (limitPerSide.has_value() &&
                result.right.size() >= limitPerSide.value()) {
                break;
            }
            result.right.push_back(contact);
        }
        return result;
    }

    [[nodiscard]] std::vector<std::shared_ptr<C>> getAllContacts() const {
        std::vector<std::shared_ptr<C>> result;
        for (const auto& [distance, contact] : this->leftNeighbors) {
            result.push_back(contact);
        }
        for (const auto& [distance, contact] : this->rightNeighbors) {
            result.push_back(contact);
        }
        return result;
    }
};

} // namespace streamr::dht::contact
