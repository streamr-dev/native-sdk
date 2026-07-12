// Module streamr.trackerlessnetwork.ContentDeliveryRpcLocal
// CONSOLIDATED from the former header logic/ContentDeliveryRpcLocal.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <optional>

#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.ContentDeliveryRpcLocal;

import streamr.trackerlessnetwork.NetworkRpcServer;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::utils::StreamPartID;
export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using ContentDeliveryRpc =
    ::streamr::protorpc::ContentDeliveryRpc<DhtCallContext>;

struct ContentDeliveryRpcLocalOptions {
    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    // TS passes `undefined` when the message has no previousMessageRef;
    // the proto3 default instance (0, 0) must not be forwarded (it
    // violates the DuplicateMessageDetector invariant when the current
    // ref is also (0, 0)).
    std::function<bool(const MessageID&, const std::optional<MessageRef>&)>
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
                message.messageid(),
                message.has_previousmessageref()
                    ? std::optional(message.previousmessageref())
                    : std::nullopt)) {
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
