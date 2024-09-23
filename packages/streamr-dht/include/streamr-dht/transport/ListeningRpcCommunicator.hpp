#ifndef STREAMR_DHT_TRANSPORT_LISTENINGRPCCOMMUNICATOR_HPP
#define STREAMR_DHT_TRANSPORT_LISTENINGRPCCOMMUNICATOR_HPP

#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/transport/RoutingRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-proto-rpc/Errors.hpp"
namespace streamr::dht::transport {

using ::dht::PeerDescriptor;
using eventemitter::HandlerToken;
using streamr::protorpc::RpcClientError;

class ListeningRpcCommunicator : public RoutingRpcCommunicator {
private:
    using RpcCommunicator::OngoingRequestBase;
    Transport& transport;
    std::function<void(const Message&)> listener;
    HandlerToken onMessageHandlerToken;
    HandlerToken onDisconnectedHandlerToken;

public:
    ListeningRpcCommunicator(
        ServiceID&& serviceId,
        Transport& transport,
        std::optional<RpcCommunicatorOptions> options = std::nullopt)
        : RoutingRpcCommunicator(
              std::move(serviceId),
              [this](const Message& message, const SendOptions& opts) {
                  this->transport.send(message, opts);
              },
              options),
          listener([this](const Message& message) {
              this->handleMessageFromPeer(message);
          }),
          transport(transport) {
        this->onMessageHandlerToken = transport.on<transportevents::Message>(
            [this](const Message& message) { this->listener(message); });
        this->onDisconnectedHandlerToken =
            transport.on<transportevents::Disconnected>(
                [this](
                    const PeerDescriptor& peerDescriptor,
                    bool /* gracefulLeave */) {
                    const auto matchingOngoingRequestIds =
                        this->getOngoingRequestIdsFulfillingPredicate(
                            [peerDescriptor](
                                const OngoingRequestBase& request) -> bool {
                                return request.getCallContext()
                                           .targetDescriptor.value()
                                           .nodeid() == peerDescriptor.nodeid();
                            });
                    for (const auto& requestId : matchingOngoingRequestIds) {
                        this->handleClientError(
                            requestId, RpcClientError("Peer disconnected"));
                    }
                });
    }

    void destroy() {
        transport.off<transportevents::Message>(this->onMessageHandlerToken);
    }
    using RpcCommunicator::registerRpcMethod;
    using RpcCommunicator::registerRpcNotification;
};

} // namespace streamr::dht::transport

#endif // STREAMR_DHT_TRANSPORT_LISTENINGRPCCOMMUNICATOR_HPP