#ifndef STREAMR_DHT_CONNECTION_CONNECTIONLOCKSTATES_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONLOCKSTATES_HPP

#include <map>
#include <set>
#include "streamr-dht/Identifiers.hpp"
#include "streamr-utils/Branded.hpp"

namespace streamr::dht::connection {

// Connection locks are independent of the existence of connections
// that is why this class is needed

using LockID = streamr::utils::Branded<std::string, struct LockIDBrand>;

class ConnectionLockStates {
private:
    std::map<DhtAddress, std::set<LockID>> localLocks;
    std::map<DhtAddress, std::set<LockID>> remoteLocks;
    std::map<DhtAddress, std::set<LockID>> weakLocks;

public:
    [[nodiscard]] size_t getLocalLockedConnectionCount() const {
        return this->localLocks.size();
    }

    [[nodiscard]] size_t getRemoteLockedConnectionCount() const {
        return this->remoteLocks.size();
    }

    [[nodiscard]] size_t getWeakLockedConnectionCount() const {
        return this->weakLocks.size();
    }

    [[nodiscard]] bool isLocalLocked(
        const DhtAddress& id,
        const std::optional<LockID>& lockId = std::nullopt) const {
        if (!lockId.has_value()) {
            return this->localLocks.find(id) != this->localLocks.end();
        }
        return this->localLocks.find(id) != this->localLocks.end() &&
            this->localLocks.at(id).find(lockId.value()) !=
            this->localLocks.at(id).end();
    }

    [[nodiscard]] bool isRemoteLocked(
        const DhtAddress& id,
        const std::optional<LockID>& lockId = std::nullopt) const {
        if (!lockId.has_value()) {
            return this->remoteLocks.find(id) != this->remoteLocks.end();
        }
        return this->remoteLocks.find(id) != this->remoteLocks.end() &&
            this->remoteLocks.at(id).find(lockId.value()) !=
            this->remoteLocks.at(id).end();
    }

    [[nodiscard]] bool isWeakLocked(const DhtAddress& id) const {
        return this->weakLocks.find(id) != this->weakLocks.end();
    }

    [[nodiscard]] bool isLocked(const DhtAddress& id) const {
        return this->isLocalLocked(id) || this->isRemoteLocked(id) ||
            this->isWeakLocked(id);
    }

    void addLocalLocked(const DhtAddress& id, const LockID& lockId) {
        if (this->localLocks.find(id) == this->localLocks.end()) {
            this->localLocks[id] = std::set<LockID>();
        }
        this->localLocks[id].insert(lockId);
    }

    void addRemoteLocked(const DhtAddress& id, const LockID& lockId) {
        if (this->remoteLocks.find(id) == this->remoteLocks.end()) {
            this->remoteLocks[id] = std::set<LockID>();
        }
        this->remoteLocks[id].insert(lockId);
    }

    void addWeakLocked(const DhtAddress& id, const LockID& lockId) {
        if (this->weakLocks.find(id) == this->weakLocks.end()) {
            this->weakLocks[id] = std::set<LockID>();
        }
        this->weakLocks[id].insert(lockId);
    }

    void removeLocalLocked(const DhtAddress& id, const LockID& lockId) {
        if (this->localLocks.find(id) != this->localLocks.end()) {
            this->localLocks[id].erase(lockId);
            if (this->localLocks[id].empty()) {
                this->localLocks.erase(id);
            }
        }
    }

    void removeRemoteLocked(const DhtAddress& id, const LockID& lockId) {
        if (this->remoteLocks.find(id) != this->remoteLocks.end()) {
            this->remoteLocks[id].erase(lockId);
            if (this->remoteLocks[id].empty()) {
                this->remoteLocks.erase(id);
            }
        }
    }

    void removeWeakLocked(const DhtAddress& id, const LockID& lockId) {
        if (this->weakLocks.find(id) != this->weakLocks.end()) {
            this->weakLocks[id].erase(lockId);
            if (this->weakLocks[id].empty()) {
                this->weakLocks.erase(id);
            }
        }
    }

    void clearAllLocks(const DhtAddress& id) {
        this->localLocks.erase(id);
        this->remoteLocks.erase(id);
        this->weakLocks.erase(id);
    }

    void clear() {
        this->localLocks.clear();
        this->remoteLocks.clear();
        this->weakLocks.clear();
    }
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_CONNECTIONLOCKSTATES_HPP