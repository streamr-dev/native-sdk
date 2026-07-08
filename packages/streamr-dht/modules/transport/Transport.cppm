// Module streamr.dht.Transport
// CONSOLIDATED from the former header streamr-dht/transport/Transport.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;
#include <new>


export module streamr.dht.Transport;

import std;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.dht.Errors;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
export namespace streamr::dht::transport {

using ::dht::PeerDescriptor;
using streamr::dht::helpers::Err;
struct SendOptions {
    bool connect = true;
    bool sendIfStopped = false;
};

namespace transportevents {

struct Disconnected : Event<PeerDescriptor, bool /*gracefulLeave*/> {};
struct Message : Event<::dht::Message> {};
struct Connected : Event<PeerDescriptor> {};

} // namespace transportevents

using TransportEvents = std::tuple<
    transportevents::Disconnected,
    transportevents::Message,
    transportevents::Connected>;

using ::dht::Message;

class Transport : public EventEmitter<TransportEvents> {
public:
    virtual void send(
        const Message& message, const SendOptions& sendOptions) = 0;
    [[nodiscard]] virtual PeerDescriptor getLocalPeerDescriptor() const = 0;
    virtual void stop() = 0;
};

} // namespace streamr::dht::transport
