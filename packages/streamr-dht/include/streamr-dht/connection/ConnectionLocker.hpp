#ifndef STREAMR_DHT_CONNECTION_CONNECTIONLOCKER_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONLOCKER_HPP

#include <cstddef>
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
    [[nodiscard]] virtual size_t getLocalLockedConnectionCount() = 0;
    [[nodiscard]] virtual size_t getRemoteLockedConnectionCount() = 0;
    [[nodiscard]] virtual size_t getWeakLockedConnectionCount() = 0;
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_CONNECTIONLOCKER_HPP