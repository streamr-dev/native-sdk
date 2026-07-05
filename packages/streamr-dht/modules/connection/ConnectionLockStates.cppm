// Module streamr.dht.ConnectionLockStates
// CONSOLIDATED from the former header
// streamr-dht/connection/ConnectionLockStates.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <map>
#include <mutex>
#include <set>

#include <string>

export module streamr.dht.ConnectionLockStates;

import streamr.utils.Branded;
import streamr.dht.Identifiers;
export namespace streamr::dht::connection {

using DhtAddress = streamr::dht::DhtAddress;
using LockID = streamr::utils::Branded<std::string, struct LockIDBrand>;

// Connection locks are independent of the existence of connections
// that is why this class is needed

class ConnectionLockStates {
private:
    std::map<DhtAddress, std::set<LockID>> localLocks;
    std::recursive_mutex localLocksMutex;
    std::map<DhtAddress, std::set<LockID>> remoteLocks;
    std::recursive_mutex remoteLocksMutex;
    std::map<DhtAddress, std::set<LockID>> weakLocks;
    std::recursive_mutex weakLocksMutex;

public:
    [[nodiscard]] size_t getLocalLockedConnectionCount() {
        std::scoped_lock lock(this->localLocksMutex);
        return this->localLocks.size();
    }

    [[nodiscard]] size_t getRemoteLockedConnectionCount() {
        std::scoped_lock lock(this->remoteLocksMutex);
        return this->remoteLocks.size();
    }

    [[nodiscard]] size_t getWeakLockedConnectionCount() {
        std::scoped_lock lock(this->weakLocksMutex);
        return this->weakLocks.size();
    }

    [[nodiscard]] bool isLocalLocked(
        const DhtAddress& id,
        const std::optional<LockID>& lockId = std::nullopt) {
        std::scoped_lock lock(this->localLocksMutex);
        if (!lockId.has_value()) {
            return this->localLocks.contains(id);
        }
        return this->localLocks.contains(id) &&
            this->localLocks.at(id).contains(lockId.value());
    }

    [[nodiscard]] bool isRemoteLocked(
        const DhtAddress& id,
        const std::optional<LockID>& lockId = std::nullopt) {
        std::scoped_lock lock(this->remoteLocksMutex);
        if (!lockId.has_value()) {
            return this->remoteLocks.contains(id);
        }
        return this->remoteLocks.contains(id) &&
            this->remoteLocks.at(id).contains(lockId.value());
    }

    [[nodiscard]] bool isWeakLocked(const DhtAddress& id) {
        std::scoped_lock lock(this->weakLocksMutex);
        return this->weakLocks.contains(id);
    }

    [[nodiscard]] bool isLocked(const DhtAddress& id) {
        std::scoped_lock lock(
            this->localLocksMutex,
            this->remoteLocksMutex,
            this->weakLocksMutex);
        return this->isLocalLocked(id) || this->isRemoteLocked(id) ||
            this->isWeakLocked(id);
    }

    void addLocalLocked(const DhtAddress& id, const LockID& lockId) {
        std::scoped_lock lock(this->localLocksMutex);
        if (!this->localLocks.contains(id)) {
            this->localLocks[id] = std::set<LockID>();
        }
        this->localLocks[id].insert(lockId);
    }

    void addRemoteLocked(const DhtAddress& id, const LockID& lockId) {
        std::scoped_lock lock(this->remoteLocksMutex);
        if (!this->remoteLocks.contains(id)) {
            this->remoteLocks[id] = std::set<LockID>();
        }
        this->remoteLocks[id].insert(lockId);
    }

    void addWeakLocked(const DhtAddress& id, const LockID& lockId) {
        std::scoped_lock lock(this->weakLocksMutex);
        if (!this->weakLocks.contains(id)) {
            this->weakLocks[id] = std::set<LockID>();
        }
        this->weakLocks[id].insert(lockId);
    }

    void removeLocalLocked(const DhtAddress& id, const LockID& lockId) {
        std::scoped_lock lock(this->localLocksMutex);
        if (this->localLocks.contains(id)) {
            this->localLocks[id].erase(lockId);
            if (this->localLocks[id].empty()) {
                this->localLocks.erase(id);
            }
        }
    }

    void removeRemoteLocked(const DhtAddress& id, const LockID& lockId) {
        std::scoped_lock lock(this->remoteLocksMutex);
        if (this->remoteLocks.contains(id)) {
            this->remoteLocks[id].erase(lockId);
            if (this->remoteLocks[id].empty()) {
                this->remoteLocks.erase(id);
            }
        }
    }

    void removeWeakLocked(const DhtAddress& id, const LockID& lockId) {
        std::scoped_lock lock(this->weakLocksMutex);
        if (this->weakLocks.contains(id)) {
            this->weakLocks[id].erase(lockId);
            if (this->weakLocks[id].empty()) {
                this->weakLocks.erase(id);
            }
        }
    }

    void clearAllLocks(const DhtAddress& id) {
        std::scoped_lock lock(
            this->localLocksMutex,
            this->remoteLocksMutex,
            this->weakLocksMutex);
        this->localLocks.erase(id);
        this->remoteLocks.erase(id);
        this->weakLocks.erase(id);
    }

    void clear() {
        std::scoped_lock lock(
            this->localLocksMutex,
            this->remoteLocksMutex,
            this->weakLocksMutex);
        this->localLocks.clear();
        this->remoteLocks.clear();
        this->weakLocks.clear();
    }
};

} // namespace streamr::dht::connection
