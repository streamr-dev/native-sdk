#ifndef STREAMR_DHT_TRANSPORT_LISTENINGRPCCOMMUNICATOR_HPP
#define STREAMR_DHT_TRANSPORT_LISTENINGRPCCOMMUNICATOR_HPP

#include "streamr-dht/transport/RoutingRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"

namespace streamr::dht::transport {

using eventemitter::HandlerToken;

class ListeningRpcCommunicator : public RoutingRpcCommunicator {
private:
    Transport& transport;
    std::function<void(const Message&)> listener;
    HandlerToken onMessageHandlerToken;
public:
    ListeningRpcCommunicator(
        ServiceID&& serviceId,
        Transport& transport,
        std::optional<RpcCommunicatorOptions> options)
        : RoutingRpcCommunicator(
              std::move(serviceId),
              [this](const Message& message, SendOptions opts) {
                  this->transport.send(message, opts);
              },
              options),
          listener([this](const Message& message) {
              this->handleMessageFromPeer(message);
          }),
          transport(transport) {
        this->onMessageHandlerToken = transport.on<transportevents::Message>(
            [this](const Message& message) { this->listener(message); });
    }

    void destroy() {
        transport.off<transportevents::Message>(this->onMessageHandlerToken);
    }
    using RpcCommunicator::registerRpcNotification;
    using RpcCommunicator::registerRpcMethod;
};

} // namespace streamr::dht::transport

#endif // STREAMR_DHT_TRANSPORT_LISTENINGRPCCOMMUNICATOR_HPP