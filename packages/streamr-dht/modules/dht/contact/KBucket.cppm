// Module streamr.dht.KBucket
// Ported from the npm `k-bucket` library (v5.1.0, MIT, Tristan Slominski),
// which the TS DHT uses via `import KBucket from 'k-bucket'`. A Kademlia
// k-bucket stored as a binary tree of nodes.
//
// Sized to what the DHT actually uses (PeerManager / DhtNode): the default
// XOR distance and the default vectorClock arbiter are built in rather
// than exposed as constructor options, and the `metadata` option and the
// `toIterable` generator are omitted. The events are wired to
// streamr-eventemitter instead of Node's EventEmitter.
module;
#include <new>


export module streamr.dht.KBucket;

import std;

import streamr.eventemitter.EventEmitter;
import streamr.dht.Identifiers;
import streamr.dht.getPeerDistance;

// Hoisted from the former header (file scope, NOT exported).
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

export namespace streamr::dht::contact {

using streamr::dht::DhtAddressRaw;
using streamr::dht::helpers::getPeerDistance;

// The element type a KBucket holds: it reports its id (for the XOR metric
// and identity) and a vector clock (for the default arbiter — the larger
// vector clock wins when two contacts share an id). Replaces TS's
// `interface KBucketContact { id: DhtAddressRaw; vectorClock: number }`.
template <typename C>
concept KBucketContact = requires(const C& contact) {
    { contact.getId() } -> std::convertible_to<DhtAddressRaw>;
    { contact.getVectorClock() } -> std::integral;
};

namespace kbucketevents {

template <typename C>
struct Added : Event<std::shared_ptr<C>> {};

template <typename C>
struct Removed : Event<std::shared_ptr<C>> {};

// (incumbent, selection)
template <typename C>
struct Updated : Event<std::shared_ptr<C>, std::shared_ptr<C>> {};

// (contacts to ping, replacement candidate)
template <typename C>
struct Ping : Event<std::vector<std::shared_ptr<C>>, std::shared_ptr<C>> {};

} // namespace kbucketevents

template <typename C>
using KBucketEvents = std::tuple<
    kbucketevents::Added<C>,
    kbucketevents::Removed<C>,
    kbucketevents::Updated<C>,
    kbucketevents::Ping<C>>;

struct KBucketOptions {
    DhtAddressRaw localNodeId;
    // The number of nodes a k-bucket holds before it must split or ping.
    std::optional<std::size_t> numberOfNodesPerKBucket;
    // The number of longest-uncontacted nodes offered in a `ping` event
    // when a non-splittable bucket is full.
    std::optional<std::size_t> numberOfNodesToPing;
};

template <KBucketContact C>
class KBucket : public EventEmitter<KBucketEvents<C>> {
private:
    // A tree node: a leaf holds `contacts`; an inner node has `contacts`
    // empty-of-value (nullopt) and two children.
    struct Node {
        std::optional<std::vector<std::shared_ptr<C>>> contacts =
            std::vector<std::shared_ptr<C>>{};
        bool dontSplit = false;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

        [[nodiscard]] bool isInner() const {
            return !this->contacts.has_value();
        }
    };

    static constexpr std::size_t defaultNodesPerKBucket = 20;
    static constexpr std::size_t defaultNodesToPing = 3;

    DhtAddressRaw localNodeId;
    std::size_t numberOfNodesPerKBucket;
    std::size_t numberOfNodesToPing;
    Node root;

    // The default vectorClock arbiter: the contact with the larger vector
    // clock wins; ties go to the candidate (the more recent contact).
    static std::shared_ptr<C> arbiter(
        const std::shared_ptr<C>& incumbent,
        const std::shared_ptr<C>& candidate) {
        return incumbent->getVectorClock() > candidate->getVectorClock()
            ? incumbent
            : candidate;
    }

    // Returns the child to descend into for `id` at `bitIndex`. An id too
    // short to have the bit is treated as having a 0 bit (goes left),
    // matching the library's out-of-range/undefined byte handling.
    Node* determineNode(Node* node, const DhtAddressRaw& id, std::size_t bitIndex) {
        const std::size_t bytesDescribedByBitIndex = bitIndex >> 3U;
        const std::size_t bitIndexWithinByte = bitIndex % 8U;
        if (bytesDescribedByBitIndex >= id.size()) {
            return node->left.get();
        }
        const auto byteUnderConsideration =
            static_cast<unsigned char>(id[bytesDescribedByBitIndex]);
        if ((byteUnderConsideration & (1U << (7U - bitIndexWithinByte))) != 0) {
            return node->right.get();
        }
        return node->left.get();
    }

    // Index of the contact with `id` in a leaf node, or -1.
    static int indexOf(const Node* node, const DhtAddressRaw& id) {
        for (std::size_t i = 0; i < node->contacts->size(); ++i) {
            if ((*node->contacts)[i]->getId() == id) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    // Splits a full leaf into two children, redistributes its contacts, and
    // marks the child NOT containing the local node id as dontSplit (the
    // "far away" bucket does not split further).
    void split(Node* node, std::size_t bitIndex) {
        node->left = std::make_unique<Node>();
        node->right = std::make_unique<Node>();
        for (const auto& contact : *node->contacts) {
            this->determineNode(node, contact->getId(), bitIndex)
                ->contacts->push_back(contact);
        }
        node->contacts = std::nullopt; // mark as inner node
        Node* detNode = this->determineNode(node, this->localNodeId, bitIndex);
        Node* otherNode = (node->left.get() == detNode) ? node->right.get()
                                                        : node->left.get();
        otherNode->dontSplit = true;
    }

    // Updates the contact at `index` using the arbiter. If the incumbent is
    // kept and the candidate is a different object, nothing changes;
    // otherwise the selection replaces it at the most-recently-contacted
    // (end) position and an `updated` event is emitted.
    void update(Node* node, std::size_t index, const std::shared_ptr<C>& contact) {
        const std::shared_ptr<C> incumbent = (*node->contacts)[index];
        const std::shared_ptr<C> selection = arbiter(incumbent, contact);
        if (selection == incumbent && incumbent != contact) {
            return;
        }
        node->contacts->erase(
            node->contacts->begin() + static_cast<std::ptrdiff_t>(index));
        node->contacts->push_back(selection);
        this->template emit<kbucketevents::Updated<C>>(incumbent, selection);
    }

public:
    explicit KBucket(KBucketOptions options)
        : localNodeId(std::move(options.localNodeId)),
          numberOfNodesPerKBucket(
              options.numberOfNodesPerKBucket.value_or(defaultNodesPerKBucket)),
          numberOfNodesToPing(
              options.numberOfNodesToPing.value_or(defaultNodesToPing)) {}

    // Adds a contact. Descends to the owning leaf; updates it in place if
    // already present; otherwise appends it if there is room; otherwise
    // either pings (if the leaf cannot split) or splits and retries.
    void add(const std::shared_ptr<C>& contact) {
        std::size_t bitIndex = 0;
        Node* node = &this->root;
        while (node->isInner()) {
            node = this->determineNode(node, contact->getId(), bitIndex++);
        }
        const int index = indexOf(node, contact->getId());
        if (index >= 0) {
            this->update(node, static_cast<std::size_t>(index), contact);
            return;
        }
        if (node->contacts->size() < this->numberOfNodesPerKBucket) {
            node->contacts->push_back(contact);
            this->template emit<kbucketevents::Added<C>>(contact);
            return;
        }
        if (node->dontSplit) {
            const std::size_t pingCount =
                std::min(this->numberOfNodesToPing, node->contacts->size());
            std::vector<std::shared_ptr<C>> toPing(
                node->contacts->begin(),
                node->contacts->begin() +
                    static_cast<std::ptrdiff_t>(pingCount));
            this->template emit<kbucketevents::Ping<C>>(toPing, contact);
            return;
        }
        this->split(node, bitIndex);
        this->add(contact);
    }

    // The contact with the exact id, or nullptr.
    [[nodiscard]] std::shared_ptr<C> get(const DhtAddressRaw& id) {
        std::size_t bitIndex = 0;
        Node* node = &this->root;
        while (node->isInner()) {
            node = this->determineNode(node, id, bitIndex++);
        }
        const int index = indexOf(node, id);
        return index >= 0 ? (*node->contacts)[static_cast<std::size_t>(index)]
                          : nullptr;
    }

    // Removes the contact with the id (emitting `removed` if it was present).
    void remove(const DhtAddressRaw& id) {
        std::size_t bitIndex = 0;
        Node* node = &this->root;
        while (node->isInner()) {
            node = this->determineNode(node, id, bitIndex++);
        }
        const int index = indexOf(node, id);
        if (index >= 0) {
            const std::shared_ptr<C> contact =
                (*node->contacts)[static_cast<std::size_t>(index)];
            node->contacts->erase(node->contacts->begin() + index);
            this->template emit<kbucketevents::Removed<C>>(contact);
        }
    }

    // The up-to-`n` closest contacts to `id` by the XOR metric, nearest
    // first. Without a limit, all contacts are returned in that order.
    [[nodiscard]] std::vector<std::shared_ptr<C>> closest(
        const DhtAddressRaw& id, std::optional<std::size_t> n = std::nullopt) {
        const std::size_t limit = n.value_or(std::numeric_limits<std::size_t>::max());
        std::vector<std::shared_ptr<C>> contacts;
        std::vector<Node*> nodes{&this->root};
        std::size_t bitIndex = 0;
        while (!nodes.empty() && contacts.size() < limit) {
            Node* node = nodes.back();
            nodes.pop_back();
            if (node->isInner()) {
                Node* detNode = this->determineNode(node, id, bitIndex++);
                nodes.push_back(
                    node->left.get() == detNode ? node->right.get()
                                                : node->left.get());
                nodes.push_back(detNode);
            } else {
                contacts.insert(
                    contacts.end(),
                    node->contacts->begin(),
                    node->contacts->end());
            }
        }
        std::ranges::stable_sort(
            contacts,
            [&id](const std::shared_ptr<C>& a, const std::shared_ptr<C>& b) {
                return getPeerDistance(a->getId(), id) <
                    getPeerDistance(b->getId(), id);
            });
        if (contacts.size() > limit) {
            contacts.resize(limit);
        }
        return contacts;
    }

    // The total number of contacts held in the tree.
    [[nodiscard]] std::size_t count() {
        std::size_t count = 0;
        std::vector<Node*> nodes{&this->root};
        while (!nodes.empty()) {
            Node* node = nodes.back();
            nodes.pop_back();
            if (node->isInner()) {
                nodes.push_back(node->right.get());
                nodes.push_back(node->left.get());
            } else {
                count += node->contacts->size();
            }
        }
        return count;
    }

    // All contacts in the tree.
    [[nodiscard]] std::vector<std::shared_ptr<C>> toArray() {
        std::vector<std::shared_ptr<C>> result;
        std::vector<Node*> nodes{&this->root};
        while (!nodes.empty()) {
            Node* node = nodes.back();
            nodes.pop_back();
            if (node->isInner()) {
                nodes.push_back(node->right.get());
                nodes.push_back(node->left.get());
            } else {
                result.insert(
                    result.end(),
                    node->contacts->begin(),
                    node->contacts->end());
            }
        }
        return result;
    }
};

} // namespace streamr::dht::contact
