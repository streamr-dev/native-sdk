// Module streamr.dht.ConnectionLocker
// CONSOLIDATED from the former header
// streamr-dht/connection/ConnectionLocker.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <cstddef>

#include <string>

#include <folly/coro/Task.h>

export module streamr.dht.ConnectionLocker;

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

    // Coroutines, not blocking calls: locking awaits a lockRequest RPC
    // round-trip. The former synchronous implementation blockingWait()ed
    // for it — on the shared worker pool that is a self-deadlock once
    // poolsize concurrent joins sit inside lockConnection at once (every
    // thread waits for an RPC response only those same threads could
    // process; seen wedging the full-node teardown on the 4-thread CI
    // pool).
    virtual folly::coro::Task<void> lockConnection(
        PeerDescriptor targetDescriptor, LockID lockId) = 0;
    virtual folly::coro::Task<void> unlockConnection(
        PeerDescriptor targetDescriptor, LockID lockId) = 0;
    virtual void weakLockConnection(
        const DhtAddress& targetDescriptor, const LockID& lockId) = 0;
    virtual void weakUnlockConnection(
        const DhtAddress& targetDescriptor, const LockID& lockId) = 0;
    [[nodiscard]] virtual size_t getLocalLockedConnectionCount() = 0;
    [[nodiscard]] virtual size_t getRemoteLockedConnectionCount() = 0;
    [[nodiscard]] virtual size_t getWeakLockedConnectionCount() = 0;
};

} // namespace streamr::dht::connection
