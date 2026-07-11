// Module streamr.dht.ListeningRpcCommunicator
// CONSOLIDATED from the former header
// streamr-dht/transport/ListeningRpcCommunicator.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;
#include <functional>
#include <optional>

#include <string>

export module streamr.dht.ListeningRpcCommunicator;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.protorpc.Errors;
import streamr.protorpc.RpcCommunicator;
import streamr.dht.RoutingRpcCommunicator;
import streamr.dht.Transport;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::eventemitter::HandlerToken;
using streamr::protorpc::RpcClientError;
// Self-sufficient shorthand (was inherited textually from a
// neighboring header before consolidation).
using streamr::protorpc::RpcCommunicatorOptions;

export namespace streamr::dht::transport {

using ::dht::PeerDescriptor;

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
        // Also detach the Disconnected listener: leaving it registered
        // dangles `this` on the transport after destruction, and a live
        // listener could still add client errors while a retired
        // communicator waits to be destroyed (see
        // RecursiveOperationManager::retiredSessionCommunicators).
        transport.off<transportevents::Disconnected>(
            this->onDisconnectedHandlerToken);
    }
    using RpcCommunicator::registerRpcMethod;
    using RpcCommunicator::registerRpcNotification;
};

} // namespace streamr::dht::transport
