// Module streamr.dht.IPendingConnection
// CONSOLIDATED from the former header
// streamr-dht/connection/IPendingConnection.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <exception>
#include <memory>

export module streamr.dht.IPendingConnection;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.dht.Connection;
export namespace streamr::dht::connection {

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

// ReplayEventEmitter, not the fire-and-forget EventEmitter: the connectors
// start socket I/O inside createConnection(), but the Connected/Disconnected
// listeners are only registered afterwards, when ConnectionManager wraps the
// returned pending connection in an Endpoint (addEndpoint ->
// changeToConnectingState). On a loaded machine the entire websocket +
// streamr handshake can win that race, and onHandshakeCompleted() disarms
// the connect watchdog before emitting — with a fire-and-forget emitter the
// emission is lost and the endpoint stays in the connecting state forever
// (the CanLockConnections stall on 2-core CI runners). Replay delivers the
// missed emission to the late listener instead.
class IPendingConnection : public streamr::eventemitter::ReplayEventEmitter<
                               PendingConnectionEvents> {
public:
    ~IPendingConnection() override = default;
    virtual void onHandshakeCompleted(
        const std::shared_ptr<Connection>& connection) = 0;
    virtual void close(bool gracefulLeave) = 0;
    virtual void destroy() = 0;
    virtual const PeerDescriptor& getPeerDescriptor() const = 0;
    virtual void onError(const std::exception_ptr& error) = 0;
    // Silence this pending connection: after this it emits neither
    // Connected nor Disconnected. Used when it loses a simultaneous-connect
    // tie-break, so the eventual close of its underlying connection cannot
    // drive (or tear down) the endpoint that has moved on to the winning
    // connection.
    virtual void replaceAsDuplicate() = 0;
};

} // namespace streamr::dht::connection
