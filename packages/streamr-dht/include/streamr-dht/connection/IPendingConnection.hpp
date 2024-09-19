#ifndef STREAMR_DHT_CONNECTION_IPENDINGCONNECTION_HPP
#define STREAMR_DHT_CONNECTION_IPENDINGCONNECTION_HPP

#include <memory>
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "packages/dht/protos/DhtRpc.pb.h"

namespace streamr::dht::connection {

using ::dht::PeerDescriptor;

// Define the events for PendingConnection
namespace pendingconnectionevents {
struct Connected : streamr::eventemitter::
                       Event<PeerDescriptor, std::shared_ptr<Connection>> {};
struct Disconnected : streamr::eventemitter::Event<bool /*gracefulLeave*/> {};
} // namespace pendingconnectionevents

using PendingConnectionEvents = std::tuple<
    pendingconnectionevents::Connected,
    pendingconnectionevents::Disconnected>;

class IPendingConnection
    : public streamr::eventemitter::EventEmitter<PendingConnectionEvents> {
public:
    virtual ~IPendingConnection() = default;

    virtual void onHandshakeCompleted(
        std::shared_ptr<Connection> connection) = 0;
    virtual void close(bool gracefulLeave) = 0;
    virtual void destroy() = 0;
    virtual const PeerDescriptor& getPeerDescriptor() const = 0;
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_IPENDINGCONNECTION_HPP