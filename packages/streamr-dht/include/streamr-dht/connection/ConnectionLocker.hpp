#ifndef STREAMR_DHT_CONNECTION_CONNECTIONLOCKER_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONLOCKER_HPP

#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/ConnectionLockStates.hpp"

namespace streamr::dht::connection {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::connection::LockID;
class ConnectionLocker {
protected:
    ConnectionLocker() = default;
    virtual ~ConnectionLocker() = default;

    virtual void lockConnection(
        const PeerDescriptor& targetDescriptor, LockID&& lockId) = 0;
    virtual void unlockConnection(
        const PeerDescriptor& targetDescriptor, LockID&& lockId) = 0;
    virtual void weakLockConnection(
        const DhtAddress& targetDescriptor, LockID&& lockId) = 0;
    virtual void weakUnlockConnection(
        const DhtAddress& targetDescriptor, LockID&& lockId) = 0;
    [[nodiscard]] virtual size_t getLocalLockedConnectionCount() const = 0;
    [[nodiscard]] virtual size_t getRemoteLockedConnectionCount() const = 0;
    [[nodiscard]] virtual size_t getWeakLockedConnectionCount() const = 0;
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_CONNECTIONLOCKER_HPP