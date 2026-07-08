// Module streamr.dht.RandomContactList
// Ported from packages/dht/src/dht/contact/RandomContactList.ts
// (v103.8.0-rc.3).
module;


export module streamr.dht.RandomContactList;

import std;

import streamr.dht.ContactList;
import streamr.dht.Identifiers;

export namespace streamr::dht::contact {

using streamr::dht::DhtAddress;

// Keeps a random sample of the contacts offered to it: each new contact is
// admitted with probability `randomness`, evicting the oldest once full.
template <HasGetNodeId C>
class RandomContactList : public ContactList<C> {
private:
    double randomness;
    std::mt19937 randomGenerator{std::random_device{}()};
    std::uniform_real_distribution<double> distribution{0.0, 1.0};

public:
    RandomContactList(
        DhtAddress localNodeId,
        std::size_t maxSize,
        double randomness = 0.20) // NOLINT(readability-magic-numbers)
        : ContactList<C>(std::move(localNodeId), maxSize),
          randomness(randomness) {}

    void addContact(const std::shared_ptr<C>& contact) {
        if (this->localNodeId == contact->getNodeId()) {
            return;
        }
        if (this->contactsById.contains(contact->getNodeId())) {
            return;
        }
        const double roll = this->distribution(this->randomGenerator);
        if (roll < this->randomness) {
            if (this->getSize() == this->maxSize && this->getSize() > 0) {
                this->removeContact(this->contactIds.front());
            }
            this->contactIds.push_back(contact->getNodeId());
            this->contactsById.emplace(contact->getNodeId(), contact);
            this->template emit<contactlistevents::ContactAdded<C>>(contact);
        }
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

    [[nodiscard]] std::vector<std::shared_ptr<C>> getContacts(
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
};

} // namespace streamr::dht::contact
