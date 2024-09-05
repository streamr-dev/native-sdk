#ifndef STREAMR_DHT_TRANSPORT_ROUTINGRPCCOMMUNICATOR_HPP
#define STREAMR_DHT_TRANSPORT_ROUTINGRPCCOMMUNICATOR_HPP

#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"
#include "streamr-utils/Uuid.hpp"

namespace streamr::dht::transport {

using ::dht::Message;
using ::protorpc::RpcMessage;
using streamr::dht::ServiceID;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::SendOptions;
using streamr::protorpc::ProtoCallContext;
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::RpcCommunicatorOptions;
using streamr::utils::Uuid;

class RoutingRpcCommunicator : public RpcCommunicator {
private:
    ServiceID ownServiceId;
    std::function<void(Message, SendOptions)> sendFn;

public:
    RoutingRpcCommunicator(
        ServiceID&& ownServiceId,
        std::function<void(Message, SendOptions)>&& sendFn,
        std::optional<RpcCommunicatorOptions> options = std::nullopt)
        : RpcCommunicator(options),
          ownServiceId(std::move(ownServiceId)),
          sendFn(std::move(sendFn)) {
        this->setOutgoingMessageCallback(
            [this](
                const RpcMessage& msg,
                const std::string& /*requestId*/,
                const std::optional<ProtoCallContext>& protoCallContext) {
                PeerDescriptor targetDescriptor;
                
                auto callContext = static_cast<const DhtCallContext&>(
                    protoCallContext.value());

                // rpc call message
                if (callContext.targetDescriptor.has_value()) {
                    targetDescriptor = callContext.targetDescriptor.value();
                } else { // rpc reply message
                    targetDescriptor =
                        callContext.incomingSourceDescriptor.value();
                }

                Message message;
                message.set_messageid(Uuid::v4());
                message.set_serviceid(this->ownServiceId);
                message.mutable_rpcmessage()->CopyFrom(msg);
                message.mutable_targetdescriptor()->CopyFrom(targetDescriptor);
                
                SendOptions sendOpts;
                const auto& header = msg.header();

                if (header.find("response") != header.end()) {
                    sendOpts.connect = false;
                    sendOpts.sendIfStopped = true;
                } else {
                    if (callContext.connect.has_value() && callContext.connect.value()) {
                        sendOpts.connect = true;
                    }
                    if (callContext.sendIfStopped.has_value() && callContext.sendIfStopped.value()) {
                        sendOpts.sendIfStopped = true;
                    }
                }
                return this->sendFn(message, sendOpts);
            });
    }
    void handleMessageFromPeer(const Message& message) {
        if (message.serviceid() == this->ownServiceId &&
            message.body_case() == Message::BodyCase::kRpcMessage) {
            DhtCallContext context;
            context.incomingSourceDescriptor = message.sourcedescriptor();
            this->handleIncomingMessage(message.rpcmessage(), context);
        }
    }
    using RpcCommunicator::registerRpcNotification;
    using RpcCommunicator::registerRpcMethod;
};

} // namespace streamr::dht::transport

#endif // STREAMR_DHT_TRANSPORT_ROUTINGRPCCOMMUNICATOR_HPP