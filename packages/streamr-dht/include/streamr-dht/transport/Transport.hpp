#ifndef STREAMR_DHT_TRANSPORT_CONNECTION_HPP
#define STREAMR_DHT_TRANSPORT_CONNECTION_HPP

#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-eventemitter/EventEmitter.hpp"

namespace streamr::dht::transport {

using ::dht::PeerDescriptor;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

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
    virtual void send(const Message& message, const SendOptions& sendOptions) = 0;
    virtual PeerDescriptor getLocalPeerDescriptor() = 0;
    virtual void stop() = 0;
};

} // namespace streamr::dht::transport

#endif // STREAMR_DHT_TRANSPORT_CONNECTION_HPP