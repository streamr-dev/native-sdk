#ifndef STREAMR_DHT_TRANSPORT_ROUTINGRPCCOMMUNICATOR_HPP
#define STREAMR_DHT_TRANSPORT_ROUTINGRPCCOMMUNICATOR_HPP

#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"
#include "streamr-utils/Uuid.hpp"

namespace streamr::dht::transport {

using ::dht::Message;
using ::protorpc::RpcMessage;
using streamr::dht::ServiceID;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::SendOptions;
using RpcCommunicator = streamr::protorpc::RpcCommunicator<DhtCallContext>;
using streamr::logger::SLogger;
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
        this->setOutgoingMessageCallback([this](
                                             const RpcMessage& msg,
                                             const std::string& /*requestId*/,
                                             const DhtCallContext&
                                                 callContext) {
            SLogger::debug("Entered outgoing message callback");
            PeerDescriptor targetDescriptor;
            SLogger::debug("Declared targetDescriptor");

            // rpc call message
            if (callContext.targetDescriptor.has_value()) {
                SLogger::debug("callContext.targetDescriptor has value");
                targetDescriptor = callContext.targetDescriptor.value();
                SLogger::debug(
                    "Set targetDescriptor from callContext.targetDescriptor");
            } else { // rpc reply message
                SLogger::debug(
                    "callContext.targetDescriptor does not have value");
                if (callContext.incomingSourceDescriptor.has_value()) {
                    SLogger::debug("incomingSourceDescriptor has value");
                } else {
                    SLogger::debug(
                        "incomingSourceDescriptor does not have value");
                }

                targetDescriptor = callContext.incomingSourceDescriptor.value();
                SLogger::debug(
                    "Set targetDescriptor from callContext.incomingSourceDescriptor");
            }

            Message message;
            SLogger::debug("Created Message object");
            message.set_messageid(Uuid::v4());
            SLogger::debug("Set message ID");
            message.set_serviceid(this->ownServiceId);
            SLogger::debug("Set service ID");
            message.mutable_rpcmessage()->CopyFrom(msg);
            SLogger::debug("Copied RpcMessage to message");
            message.mutable_targetdescriptor()->CopyFrom(targetDescriptor);
            SLogger::debug("Copied targetDescriptor to message");

            SendOptions sendOpts;
            SLogger::debug("Created SendOptions object");
            const auto& header = msg.header();
            SLogger::debug("Retrieved message header");

            if (header.find("response") != header.end()) {
                sendOpts.connect = false;
                sendOpts.sendIfStopped = true;
                SLogger::debug("Set sendOpts for response message");
            } else {
                if (callContext.connect.has_value() &&
                    callContext.connect.value()) {
                    sendOpts.connect = true;
                    SLogger::debug("Set sendOpts.connect to true");
                }
                if (callContext.sendIfStopped.has_value() &&
                    callContext.sendIfStopped.value()) {
                    sendOpts.sendIfStopped = true;
                    SLogger::debug("Set sendOpts.sendIfStopped to true");
                }
            }
            SLogger::debug("Calling sendFn with message and sendOpts");
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
    using RpcCommunicator::registerRpcMethod;
    using RpcCommunicator::registerRpcNotification;
};

} // namespace streamr::dht::transport

#endif // STREAMR_DHT_TRANSPORT_ROUTINGRPCCOMMUNICATOR_HPP