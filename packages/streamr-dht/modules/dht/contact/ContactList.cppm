// Module streamr.dht.ContactList
// Ported from packages/dht/src/dht/contact/ContactList.ts (v103.8.0-rc.3).
module;
#include <new>


export module streamr.dht.ContactList;

import std;

import streamr.eventemitter.EventEmitter;
import streamr.dht.Identifiers;

// Hoisted from the former header (file scope, NOT exported).
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

export namespace streamr::dht::contact {

using streamr::dht::DhtAddress;

// The element type the sorted/random contact lists hold: anything that can
// report its node id. Replaces TS's `C extends { getNodeId: () => DhtAddress
// }`.
template <typename C>
concept HasGetNodeId = requires(const C& contact) {
    { contact.getNodeId() } -> std::convertible_to<DhtAddress>;
};

// The events every contact list emits. The TS `Events<C>` interface is
// shared by ContactList and SortedContactList; here it is the event tuple
// both extend. Contacts are passed by shared_ptr (C is a class type with
// getNodeId()).
namespace contactlistevents {

template <typename C>
struct ContactRemoved : Event<std::shared_ptr<C>> {};

template <typename C>
struct ContactAdded : Event<std::shared_ptr<C>> {};

} // namespace contactlistevents

template <typename C>
using ContactListEvents = std::tuple<
    contactlistevents::ContactRemoved<C>,
    contactlistevents::ContactAdded<C>>;

template <HasGetNodeId C>
class ContactList : public EventEmitter<ContactListEvents<C>> {
protected:
    std::map<DhtAddress, std::shared_ptr<C>> contactsById;
    // TODO (from TS) move this to SortedContactList
    std::vector<DhtAddress> contactIds;
    DhtAddress localNodeId;
    std::size_t maxSize;

public:
    ContactList(DhtAddress localNodeId, std::size_t maxSize)
        : localNodeId(std::move(localNodeId)), maxSize(maxSize) {}

    ~ContactList() override = default;

    [[nodiscard]] std::shared_ptr<C> getContact(const DhtAddress& id) const {
        const auto it = this->contactsById.find(id);
        if (it == this->contactsById.end()) {
            return nullptr;
        }
        return it->second;
    }

    [[nodiscard]] std::size_t getSize() const { return this->contactIds.size(); }

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
