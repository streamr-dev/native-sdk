#ifndef STREAMR_TRACKERLESS_NETWORK_CONTENT_DELIVERY_RPC_LOCAL_HPP
#define STREAMR_TRACKERLESS_NETWORK_CONTENT_DELIVERY_RPC_LOCAL_HPP

#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "packages/network/protos/NetworkRpc.server.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"
#include "streamr-utils/StreamPartID.hpp"

namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::utils::StreamPartID;
using ContentDeliveryRpc =
    ::streamr::protorpc::ContentDeliveryRpc<DhtCallContext>;

struct ContentDeliveryRpcLocalOptions {
    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    std::function<bool(const MessageID&, const MessageRef&)>
        markAndCheckDuplicate;
    std::function<void(const StreamMessage&, const DhtAddress&)> broadcast;
    std::function<void(const DhtAddress&, bool)> onLeaveNotice;
    std::function<void(const DhtAddress&, const MessageID&)> markForInspection;
    ListeningRpcCommunicator& rpcCommunicator;
};

class ContentDeliveryRpcLocal : public ContentDeliveryRpc {
private:
    ContentDeliveryRpcLocalOptions options;

public:
    explicit ContentDeliveryRpcLocal(ContentDeliveryRpcLocalOptions options)
        : options(std::move(options)) {}

    void sendStreamMessage(
        const StreamMessage& message, const DhtCallContext& context) override {
        const auto previousNode = Identifiers::getNodeIdFromPeerDescriptor(
            context.incomingSourceDescriptor.value());
        this->options.markForInspection(previousNode, message.messageid());
        if (this->options.markAndCheckDuplicate(
                message.messageid(), message.previousmessageref())) {
            this->options.broadcast(message, previousNode);
        }
    }

    void leaveStreamPartNotice(
        const LeaveStreamPartNotice& message,
        const DhtCallContext& context) override {
        if (message.streampartid() == this->options.streamPartId) {
            const auto sourcePeerDescriptor =
                context.incomingSourceDescriptor.value();
            const auto remoteNodeId =
                Identifiers::getNodeIdFromPeerDescriptor(sourcePeerDescriptor);
            this->options.onLeaveNotice(remoteNodeId, message.isentrypoint());
        }
    }
};

} // namespace streamr::trackerlessnetwork

#endif // STREAMR_TRACKERLESS_NETWORK_CONTENT_DELIVERY_RPC_LOCAL_HPP
