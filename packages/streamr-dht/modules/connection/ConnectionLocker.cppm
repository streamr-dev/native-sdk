// Module streamr.dht.ConnectionLocker
// CONSOLIDATED from the former header
// streamr-dht/connection/ConnectionLocker.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;
#include <new>



export module streamr.dht.ConnectionLocker;

import std;

import streamr.dht.protos;

import streamr.dht.ConnectionLockStates;
import streamr.dht.Identifiers;
export namespace streamr::dht::connection {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::connection::LockID;
class ConnectionLocker {
protected:
    ConnectionLocker() = default;

public:
    virtual ~ConnectionLocker() = default;

    virtual void lockConnection(
        PeerDescriptor targetDescriptor, LockID lockId) = 0;
    virtual void unlockConnection(
        PeerDescriptor targetDescriptor, LockID lockId) = 0;
    virtual void weakLockConnection(
        const DhtAddress& targetDescriptor, const LockID& lockId) = 0;
    virtual void weakUnlockConnection(
        const DhtAddress& targetDescriptor, const LockID& lockId) = 0;
    [[nodiscard]] virtual std::size_t getLocalLockedConnectionCount() = 0;
    [[nodiscard]] virtual std::size_t getRemoteLockedConnectionCount() = 0;
    [[nodiscard]] virtual std::size_t getWeakLockedConnectionCount() = 0;
};

} // namespace streamr::dht::connection
