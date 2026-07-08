// Module streamr.dht.SortedContactList
// Ported from packages/dht/src/dht/contact/SortedContactList.ts
// (v103.8.0-rc.3). Unlike RandomContactList it does NOT extend
// ContactList — it keeps its own contactsById/contactIds and only reuses
// ContactList's event tuple (matching TS).
module;


export module streamr.dht.SortedContactList;

import std;

import streamr.eventemitter.EventEmitter;
import streamr.dht.ContactList;
import streamr.dht.Identifiers;
import streamr.dht.getPeerDistance;

// Hoisted from the former header (file scope, NOT exported).
using streamr::eventemitter::EventEmitter;

export namespace streamr::dht::contact {

using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::dht::helpers::getPeerDistance;

struct SortedContactListOptions {
    // all contacts in this list are sorted by the distance to this id
    DhtAddress referenceId;
    bool allowToContainReferenceId;
    std::optional<std::size_t> maxSize;
    // if set, the list can't contain contacts further away than this limit
    std::optional<DhtAddress> nodeIdDistanceLimit;
    // if set, the list can't contain contacts with these ids
    std::optional<std::set<DhtAddress>> excludedNodeIds;
};

template <HasGetNodeId C>
class SortedContactList : public EventEmitter<ContactListEvents<C>> {
private:
    SortedContactListOptions options;
    DhtAddressRaw referenceIdRaw;
    std::map<DhtAddress, std::shared_ptr<C>> contactsById;
    std::vector<DhtAddress> contactIds; // sorted ascending by distance

    [[nodiscard]] double distanceToReferenceId(const DhtAddress& id) const {
        return getPeerDistance(
            this->referenceIdRaw, Identifiers::getRawFromDhtAddress(id));
    }

    // Lowest index at which contactId keeps contactIds sorted ascending by
    // distance (lodash sortedIndexBy: leftmost position for equal keys).
    [[nodiscard]] std::size_t sortedInsertionIndex(
        const DhtAddress& contactId) const {
        const double distance = this->distanceToReferenceId(contactId);
        const auto it = std::lower_bound(
            this->contactIds.begin(),
            this->contactIds.end(),
            distance,
            [this](const DhtAddress& existing, double value) {
                return this->distanceToReferenceId(existing) < value;
            });
        return static_cast<std::size_t>(it - this->contactIds.begin());
    }

public:
    explicit SortedContactList(SortedContactListOptions options)
        : options(std::move(options)),
          referenceIdRaw(
              Identifiers::getRawFromDhtAddress(this->options.referenceId)) {}

    [[nodiscard]] DhtAddress getClosestContactId() const {
        return this->contactIds.front();
    }

    [[nodiscard]] std::vector<DhtAddress> getContactIds() const {
        return this->contactIds;
    }

    void addContact(const std::shared_ptr<C>& contact) {
        const DhtAddress contactId = contact->getNodeId();
        if (this->options.excludedNodeIds.has_value() &&
            this->options.excludedNodeIds->contains(contactId)) {
            return;
        }
        if ((!this->options.allowToContainReferenceId &&
             this->options.referenceId == contactId) ||
            (this->options.nodeIdDistanceLimit.has_value() &&
             this->compareIds(
                 this->options.nodeIdDistanceLimit.value(), contactId) < 0)) {
            return;
        }
        if (this->contactsById.contains(contactId)) {
            return;
        }
        if (!this->options.maxSize.has_value() ||
            this->contactIds.size() < this->options.maxSize.value()) {
            this->contactsById.emplace(contactId, contact);
            this->contactIds.insert(
                this->contactIds.begin() +
                    static_cast<std::ptrdiff_t>(
                        this->sortedInsertionIndex(contactId)),
                contactId);
            this->template emit<contactlistevents::ContactAdded<C>>(contact);
        } else if (
            this->compareIds(
                this->contactIds[this->options.maxSize.value() - 1],
                contactId) > 0) {
            const DhtAddress removedId = this->contactIds.back();
            this->contactIds.pop_back();
            const std::shared_ptr<C> removedContact =
                this->contactsById.at(removedId);
            this->contactsById.erase(removedId);
            this->contactsById.emplace(contactId, contact);
            this->contactIds.insert(
                this->contactIds.begin() +
                    static_cast<std::ptrdiff_t>(
                        this->sortedInsertionIndex(contactId)),
                contactId);
            this->template emit<contactlistevents::ContactRemoved<C>>(
                removedContact);
            this->template emit<contactlistevents::ContactAdded<C>>(contact);
        }
    }

    void addContacts(const std::vector<std::shared_ptr<C>>& contacts) {
        for (const auto& contact : contacts) {
            this->addContact(contact);
        }
    }

    [[nodiscard]] std::shared_ptr<C> getContact(const DhtAddress& id) const {
        const auto it = this->contactsById.find(id);
        if (it == this->contactsById.end()) {
            return nullptr;
        }
        return it->second;
    }

    [[nodiscard]] bool has(const DhtAddress& id) const {
        return this->contactsById.contains(id);
    }

    // Closest first, then others in ascending distance order.
    [[nodiscard]] std::vector<std::shared_ptr<C>> getClosestContacts(
        std::optional<int> limit = std::nullopt) const {
        const std::size_t count = limit.has_value()
            ? std::min(
                  this->contactIds.size(),
                  static_cast<std::size_t>(std::max(limit.value(), 0)))
            : this->contactIds.size();
        std::vector<std::shared_ptr<C>> result;
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            result.push_back(this->contactsById.at(this->contactIds[i]));
        }
        return result;
    }

    // Furthest first, then others in descending distance order.
    [[nodiscard]] std::vector<std::shared_ptr<C>> getFurthestContacts(
        std::optional<int> limit = std::nullopt) const {
        std::vector<std::shared_ptr<C>> reversed = this->getClosestContacts();
        std::ranges::reverse(reversed);
        if (!limit.has_value()) {
            return reversed;
        }
        const std::size_t count = std::min(
            reversed.size(), static_cast<std::size_t>(std::max(limit.value(), 0)));
        reversed.resize(count);
        return reversed;
    }

    [[nodiscard]] double compareIds(
        const DhtAddress& id1, const DhtAddress& id2) const {
        return this->distanceToReferenceId(id1) -
            this->distanceToReferenceId(id2);
    }

    bool removeContact(const DhtAddress& id) {
        const auto it = this->contactsById.find(id);
        if (it == this->contactsById.end()) {
            return false;
        }
        const std::shared_ptr<C> removed = it->second;
        const auto idIt = std::ranges::find(this->contactIds, id);
        if (idIt != this->contactIds.end()) {
            this->contactIds.erase(idIt);
        }
        this->contactsById.erase(it);
        this->template emit<contactlistevents::ContactRemoved<C>>(removed);
        return true;
    }

    [[nodiscard]] std::vector<std::shared_ptr<C>>
    getAllContactsInUndefinedOrder() const {
        std::vector<std::shared_ptr<C>> result;
        result.reserve(this->contactsById.size());
        for (const auto& [id, contact] : this->contactsById) {
            result.push_back(contact);
        }
        return result;
    }

    [[nodiscard]] std::size_t getSize(
        const std::optional<std::set<DhtAddress>>& excludedNodeIds =
            std::nullopt) const {
        std::size_t excludedCount = 0;
        if (excludedNodeIds.has_value()) {
            for (const auto& nodeId : excludedNodeIds.value()) {
                if (this->has(nodeId)) {
                    ++excludedCount;
                }
            }
        }
        return this->contactIds.size() - excludedCount;
    }

    void clear() {
        this->contactsById.clear();
        this->contactIds.clear();
    }

    void stop() {
        this->removeAllListeners();
        this->clear();
    }
};

} // namespace streamr::dht::contact
