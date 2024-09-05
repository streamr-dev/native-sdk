#ifndef STREAMR_DHT_CONNECTION_CONNECTIONLOCKER_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONLOCKER_HPP

#include "streamr-dht/connection/ConnectionLockStates.hpp"
#include "packages/dht/protos/DhtRpc.pb.h"

namespace streamr::dht::connection {

using ::dht::PeerDescriptor;
using streamr::dht::connection::LockID;

class ConnectionLocker {
    protected:
        ConnectionLocker() = default;
        virtual ~ConnectionLocker() = default;

    virtual void lockConnection(const PeerDescriptor& targetDescriptor, const LockID& lockId) = 0;
    virtual void unlockConnection(const PeerDescriptor& targetDescriptor, const LockID& lockId) = 0;
    virtual void weakLockConnection(const DhtAddress& nodeId, const LockID& lockId) = 0;
    virtual void weakUnlockConnection(const DhtAddress& nodeId, const LockID& lockId) = 0;
    [[nodiscard]] virtual size_t getLocalLockedConnectionCount() const = 0;
    [[nodiscard]] virtual size_t getRemoteLockedConnectionCount() const = 0;
    [[nodiscard]] virtual size_t getWeakLockedConnectionCount() const = 0;
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_CONNECTIONLOCKER_HPP