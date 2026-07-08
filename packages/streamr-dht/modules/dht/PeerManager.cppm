// Module streamr.dht.PeerManager
// Ported from packages/dht/src/dht/PeerManager.ts (v103.8.0-rc.3).
//
// Manages the node's view of the network: the k-bucket 'neighbors', the
// distance-sorted 'nearbyContacts', the 'randomContacts', the ring
// 'ringContacts', and the set of 'activeContacts'. When a contact is added
// to the k-bucket it is pinged; a contact that fails the ping is dropped
// and replaced by the closest active nearby contact.
//
// Threading note (a C++ design point, since TS is single-threaded): the
// ping is fired off the calling thread via AbortableTimers and its result
// handled later, so every operation (and the k-bucket/contact-list event
// handlers) runs under one recursive mutex; PeerManager is owned through a
// shared_ptr so the deferred ping handler can pin it.
module;



export module streamr.dht.PeerManager;

import std;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.utils.AbortController;
import streamr.utils.AbortableTimers;
import streamr.utils.CoroutineHelper;
import streamr.utils.EnableSharedFromThis;
import streamr.logger.SLogger;
import streamr.dht.ConnectionLocker;
import streamr.dht.ConnectionLockStates;
import streamr.dht.ContactList;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.Identifiers;
import streamr.dht.KBucket;
import streamr.dht.RandomContactList;
import streamr.dht.RingContactList;
import streamr.dht.ringIdentifiers;
import streamr.dht.SortedContactList;

// Hoisted from the former header (file scope, NOT exported).
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::AbortableTimers;
using streamr::utils::AbortController;
using streamr::utils::EnableSharedFromThis;

export namespace streamr::dht {

using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::LockID;
using streamr::dht::contact::getRingIdRawFromPeerDescriptor;
using streamr::dht::contact::KBucket;
using streamr::dht::contact::KBucketOptions;
using streamr::dht::contact::RandomContactList;
using streamr::dht::contact::RingContactList;
using streamr::dht::contact::RingContacts;
using streamr::dht::contact::RingIdRaw;
using streamr::dht::contact::SortedContactList;
using streamr::dht::contact::SortedContactListOptions;

namespace kbucketevents = streamr::dht::contact::kbucketevents;
namespace contactlistevents = streamr::dht::contact::contactlistevents;

namespace peermanagerevents {
struct NearbyContactAdded : Event<PeerDescriptor> {};
struct NearbyContactRemoved : Event<PeerDescriptor> {};
struct RandomContactAdded : Event<PeerDescriptor> {};
struct RandomContactRemoved : Event<PeerDescriptor> {};
struct RingContactAdded : Event<PeerDescriptor> {};
struct RingContactRemoved : Event<PeerDescriptor> {};
struct KBucketEmpty : Event<> {};
} // namespace peermanagerevents

using PeerManagerEvents = std::tuple<
    peermanagerevents::NearbyContactAdded,
    peermanagerevents::NearbyContactRemoved,
    peermanagerevents::RandomContactAdded,
    peermanagerevents::RandomContactRemoved,
    peermanagerevents::RingContactAdded,
    peermanagerevents::RingContactRemoved,
    peermanagerevents::KBucketEmpty>;

struct PeerManagerOptions {
    std::size_t numberOfNodesPerKBucket;
    std::size_t maxContactCount;
    DhtAddress localNodeId;
    PeerDescriptor localPeerDescriptor;
    std::shared_ptr<ConnectionLocker> connectionLocker; // may be null
    std::optional<std::size_t> neighborPingLimit;
    LockID lockId;
    std::function<std::shared_ptr<DhtNodeRpcRemote>(const PeerDescriptor&)>
        createDhtNodeRpcRemote;
    std::function<bool(const DhtAddress&)> hasConnection;
};

class PeerManager : public EventEmitter<PeerManagerEvents>,
                    public EnableSharedFromThis {
private:
    PeerManagerOptions options;
    std::recursive_mutex mutex;
    AbortController abortController;
    bool stopped = false;
    std::set<DhtAddress> activeContacts;

    std::unique_ptr<KBucket<DhtNodeRpcRemote>> neighbors;
    std::unique_ptr<SortedContactList<DhtNodeRpcRemote>> nearbyContacts;
    std::unique_ptr<RingContactList<DhtNodeRpcRemote>> ringContacts;
    std::unique_ptr<RandomContactList<DhtNodeRpcRemote>> randomContacts;

    void weakUnlock(const DhtAddress& nodeId) {
        if (this->options.connectionLocker) {
            this->options.connectionLocker->weakUnlockConnection(
                nodeId, this->options.lockId);
        }
    }

    void onKBucketPing(
        const std::vector<std::shared_ptr<DhtNodeRpcRemote>>& oldContacts,
        const std::shared_ptr<DhtNodeRpcRemote>& newContact) {
        if (this->stopped) {
            return;
        }
        SortedContactList<DhtNodeRpcRemote> sortingList(
            SortedContactListOptions{
                .referenceId = this->options.localNodeId,
                .allowToContainReferenceId = false});
        sortingList.addContacts(oldContacts);
        const auto furthest = sortingList.getFurthestContacts(1);
        if (furthest.empty()) {
            return;
        }
        const DhtAddress removableNodeId = furthest[0]->getNodeId();
        this->weakUnlock(removableNodeId);
        this->neighbors->remove(
            Identifiers::getRawFromDhtAddress(removableNodeId));
        this->neighbors->add(newContact);
    }

    void onKBucketRemoved(const DhtAddress& nodeId) {
        if (this->stopped) {
            return;
        }
        this->weakUnlock(nodeId);
        SLogger::trace("Removed contact " + nodeId);
        if (this->neighbors->count() == 0) {
            this->emit<peermanagerevents::KBucketEmpty>();
        }
    }

    void onKBucketAdded(const std::shared_ptr<DhtNodeRpcRemote>& contact) {
        if (this->stopped) {
            return;
        }
        if (contact->getNodeId() == this->options.localNodeId) {
            return;
        }
        const auto peerDescriptor = contact->getPeerDescriptor();
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        // Lock before the ping result is known, matching TS.
        if (this->options.connectionLocker) {
            this->options.connectionLocker->weakLockConnection(
                nodeId, this->options.lockId);
        }
        if (this->options.hasConnection(contact->getNodeId()) ||
            (this->options.neighborPingLimit.has_value() &&
             this->neighbors->count() >
                 this->options.neighborPingLimit.value())) {
            SLogger::trace("Added new contact " + nodeId);
            return;
        }
        this->schedulePing(contact, nodeId);
    }

    // Fires the ping off this thread; on failure drops the contact and pulls
    // in the closest active nearby contact.
    void schedulePing(
        const std::shared_ptr<DhtNodeRpcRemote>& contact,
        const DhtAddress& nodeId) {
        auto self = this->sharedFromThis<PeerManager>();
        AbortableTimers::setAbortableTimeout(
            [self, contact, nodeId]() {
                bool result = false;
                try {
                    result = streamr::utils::blockingWait(contact->ping());
                } catch (const std::exception& err) {
                    SLogger::trace("ping threw " + std::string(err.what()));
                    result = false;
                }
                std::scoped_lock lock(self->mutex);
                if (self->stopped) {
                    return;
                }
                if (result) {
                    SLogger::trace("Added new contact " + nodeId);
                } else {
                    SLogger::trace("ping failed " + nodeId);
                    self->weakUnlock(nodeId);
                    self->removeContact(nodeId);
                    self->addNearbyContactToNeighbors();
                }
            },
            std::chrono::milliseconds(0),
            self->abortController.getSignal());
    }

    void addNearbyContactToNeighbors() {
        if (this->stopped) {
            return;
        }
        const auto closest = this->getNearbyActiveContactNotInNeighbors();
        if (closest) {
            this->addContact(closest->getPeerDescriptor());
        }
    }

    std::shared_ptr<DhtNodeRpcRemote> getNearbyActiveContactNotInNeighbors() {
        for (const auto& contactId : this->nearbyContacts->getContactIds()) {
            if (!this->neighbors->get(
                    Identifiers::getRawFromDhtAddress(contactId)) &&
                this->activeContacts.contains(contactId)) {
                return this->nearbyContacts->getContact(contactId);
            }
        }
        return nullptr;
    }

    void registerEventHandlers() {
        this->ringContacts
            ->on<contactlistevents::ContactAdded<DhtNodeRpcRemote>>(
                [this](const std::shared_ptr<DhtNodeRpcRemote>& contact) {
                    this->emit<peermanagerevents::RingContactAdded>(
                        contact->getPeerDescriptor());
                });
        this->ringContacts
            ->on<contactlistevents::ContactRemoved<DhtNodeRpcRemote>>(
                [this](const std::shared_ptr<DhtNodeRpcRemote>& contact) {
                    this->emit<peermanagerevents::RingContactRemoved>(
                        contact->getPeerDescriptor());
                });

        this->neighbors->on<kbucketevents::Ping<DhtNodeRpcRemote>>(
            [this](
                const std::vector<std::shared_ptr<DhtNodeRpcRemote>>&
                    oldContacts,
                const std::shared_ptr<DhtNodeRpcRemote>& newContact) {
                this->onKBucketPing(oldContacts, newContact);
            });
        this->neighbors->on<kbucketevents::Removed<DhtNodeRpcRemote>>(
            [this](const std::shared_ptr<DhtNodeRpcRemote>& contact) {
                this->onKBucketRemoved(
                    Identifiers::getNodeIdFromPeerDescriptor(
                        contact->getPeerDescriptor()));
            });
        this->neighbors->on<kbucketevents::Added<DhtNodeRpcRemote>>(
            [this](const std::shared_ptr<DhtNodeRpcRemote>& contact) {
                this->onKBucketAdded(contact);
            });

        this->nearbyContacts
            ->on<contactlistevents::ContactRemoved<DhtNodeRpcRemote>>(
                [this](const std::shared_ptr<DhtNodeRpcRemote>& contact) {
                    if (this->stopped) {
                        return;
                    }
                    this->emit<peermanagerevents::NearbyContactRemoved>(
                        contact->getPeerDescriptor());
                    this->randomContacts->addContact(
                        this->options.createDhtNodeRpcRemote(
                            contact->getPeerDescriptor()));
                });
        this->nearbyContacts
            ->on<contactlistevents::ContactAdded<DhtNodeRpcRemote>>(
                [this](const std::shared_ptr<DhtNodeRpcRemote>& contact) {
                    this->emit<peermanagerevents::NearbyContactAdded>(
                        contact->getPeerDescriptor());
                });

        this->randomContacts
            ->on<contactlistevents::ContactRemoved<DhtNodeRpcRemote>>(
                [this](const std::shared_ptr<DhtNodeRpcRemote>& contact) {
                    this->emit<peermanagerevents::RandomContactRemoved>(
                        contact->getPeerDescriptor());
                });
        this->randomContacts
            ->on<contactlistevents::ContactAdded<DhtNodeRpcRemote>>(
                [this](const std::shared_ptr<DhtNodeRpcRemote>& contact) {
                    this->emit<peermanagerevents::RandomContactAdded>(
                        contact->getPeerDescriptor());
                });
    }

    explicit PeerManager(PeerManagerOptions options)
        : options(std::move(options)) {
        this->neighbors =
            std::make_unique<KBucket<DhtNodeRpcRemote>>(KBucketOptions{
                .localNodeId = Identifiers::getRawFromDhtAddress(
                    this->options.localNodeId),
                .numberOfNodesPerKBucket =
                    this->options.numberOfNodesPerKBucket,
                .numberOfNodesToPing = this->options.numberOfNodesPerKBucket});
        this->ringContacts =
            std::make_unique<RingContactList<DhtNodeRpcRemote>>(
                getRingIdRawFromPeerDescriptor(
                    this->options.localPeerDescriptor));
        this->nearbyContacts =
            std::make_unique<SortedContactList<DhtNodeRpcRemote>>(
                SortedContactListOptions{
                    .referenceId = this->options.localNodeId,
                    .allowToContainReferenceId = false,
                    .maxSize = this->options.maxContactCount});
        this->randomContacts =
            std::make_unique<RandomContactList<DhtNodeRpcRemote>>(
                this->options.localNodeId, this->options.maxContactCount);
    }

public:
    [[nodiscard]] static std::shared_ptr<PeerManager> newInstance(
        PeerManagerOptions options) {
        struct MakeSharedEnabler : public PeerManager {
            explicit MakeSharedEnabler(PeerManagerOptions options)
                : PeerManager(std::move(options)) {}
        };
        auto instance = std::make_shared<MakeSharedEnabler>(std::move(options));
        instance->registerEventHandlers();
        return instance;
    }

    ~PeerManager() override = default;

    void removeContact(const DhtAddress& nodeId) {
        std::scoped_lock lock(this->mutex);
        if (this->stopped) {
            return;
        }
        SLogger::trace("Removing contact " + nodeId);
        this->ringContacts->removeContact(
            this->nearbyContacts->getContact(nodeId));
        this->neighbors->remove(Identifiers::getRawFromDhtAddress(nodeId));
        this->nearbyContacts->removeContact(nodeId);
        this->activeContacts.erase(nodeId);
        this->randomContacts->removeContact(nodeId);
    }

    void removeNeighbor(const DhtAddress& nodeId) {
        std::scoped_lock lock(this->mutex);
        this->neighbors->remove(Identifiers::getRawFromDhtAddress(nodeId));
    }

    // Pings the given nodes and removes the ones that do not answer, marking
    // the responders active. TS's pingNodes helper pings in parallel
    // (Promise.allSettled); here the pings are awaited one at a time, which
    // yields the same offline set and the same activeContacts updates. The
    // mutex is never held across a co_await.
    folly::coro::Task<void> pruneOfflineNodes(
        std::vector<std::shared_ptr<DhtNodeRpcRemote>> nodes) {
        SLogger::trace("Pruning offline nodes");
        std::vector<PeerDescriptor> offlineNeighbors;
        for (const auto& contact : nodes) {
            bool isOnline = false;
            try {
                isOnline = co_await contact->ping();
            } catch (const std::exception&) {
                isOnline = false;
            }
            std::scoped_lock lock(this->mutex);
            if (isOnline) {
                this->activeContacts.insert(contact->getNodeId());
            } else {
                this->activeContacts.erase(contact->getNodeId());
                offlineNeighbors.push_back(contact->getPeerDescriptor());
            }
        }
        std::scoped_lock lock(this->mutex);
        for (const auto& offlineNeighbor : offlineNeighbors) {
            this->removeContact(
                Identifiers::getNodeIdFromPeerDescriptor(offlineNeighbor));
        }
        co_return;
    }

    void stop() {
        std::scoped_lock lock(this->mutex);
        this->stopped = true;
        this->abortController.abort();
        for (const auto& rpcRemote : this->neighbors->toArray()) {
            rpcRemote->leaveNotice();
            this->neighbors->remove(rpcRemote->getId());
        }
        this->neighbors->removeAllListeners();
        for (const auto& rpcRemote : this->ringContacts->getAllContacts()) {
            rpcRemote->leaveNotice();
            this->ringContacts->removeContact(rpcRemote);
        }
        this->nearbyContacts->stop();
        this->randomContacts->stop();
    }

    [[nodiscard]] RingContacts<DhtNodeRpcRemote> getClosestRingContactsTo(
        const RingIdRaw& ringIdRaw,
        std::optional<std::size_t> limit = std::nullopt,
        std::optional<std::set<DhtAddress>> excludedIds = std::nullopt) {
        std::scoped_lock lock(this->mutex);
        RingContactList<DhtNodeRpcRemote> closest(ringIdRaw, excludedIds);
        for (const auto& contact : this->ringContacts->getAllContacts()) {
            closest.addContact(contact);
        }
        constexpr std::size_t defaultLimit = 8;
        return closest.getClosestContacts(limit.value_or(defaultLimit));
    }

    [[nodiscard]] std::size_t getNearbyContactCount(
        const std::optional<std::set<DhtAddress>>& excludedNodeIds =
            std::nullopt) {
        std::scoped_lock lock(this->mutex);
        return this->nearbyContacts->getSize(excludedNodeIds);
    }

    // TS returns the SortedContactList itself; the only consumer
    // (DiscoverySession) maps it straight to peer descriptors, so this returns
    // the descriptors directly and keeps the list access under the mutex.
    [[nodiscard]] std::vector<PeerDescriptor> getNearbyContacts() {
        std::scoped_lock lock(this->mutex);
        const auto contacts =
            this->nearbyContacts->getAllContactsInUndefinedOrder();
        std::vector<PeerDescriptor> result;
        result.reserve(contacts.size());
        for (const auto& contact : contacts) {
            result.push_back(contact->getPeerDescriptor());
        }
        return result;
    }

    [[nodiscard]] std::size_t getNeighborCount() {
        std::scoped_lock lock(this->mutex);
        return this->neighbors->count();
    }

    [[nodiscard]] std::vector<std::shared_ptr<DhtNodeRpcRemote>>
    getNeighbors() {
        std::scoped_lock lock(this->mutex);
        return this->neighbors->toArray();
    }

    void setContactActive(const DhtAddress& nodeId) {
        std::scoped_lock lock(this->mutex);
        this->activeContacts.insert(nodeId);
    }

    void addContact(const PeerDescriptor& peerDescriptor) {
        std::scoped_lock lock(this->mutex);
        if (this->stopped) {
            return;
        }
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        if (nodeId == this->options.localNodeId) {
            return;
        }
        SLogger::trace("Adding new contact " + nodeId);
        const auto remote =
            this->options.createDhtNodeRpcRemote(peerDescriptor);
        const bool isInNeighbors =
            this->neighbors->get(DhtAddressRaw{peerDescriptor.nodeid()}) !=
            nullptr;
        const bool isInNearbyContacts =
            this->nearbyContacts->getContact(nodeId) != nullptr;
        const bool isInRingContacts =
            this->ringContacts->getContact(peerDescriptor) != nullptr;

        if (isInNeighbors || isInNearbyContacts) {
            this->randomContacts->addContact(remote);
        }
        if (!isInNeighbors) {
            this->neighbors->add(remote);
        }
        if (!isInNearbyContacts) {
            this->nearbyContacts->addContact(remote);
        }
        if (!isInRingContacts) {
            this->ringContacts->addContact(remote);
        }
    }
};

} // namespace streamr::dht
