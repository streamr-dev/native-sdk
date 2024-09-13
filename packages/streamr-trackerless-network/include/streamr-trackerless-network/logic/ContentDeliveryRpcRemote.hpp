#ifndef STREAMR_TRACKERLESS_NETWORK_CONTENT_DELIVERY_RPC_REMOTE_HPP
#define STREAMR_TRACKERLESS_NETWORK_CONTENT_DELIVERY_RPC_REMOTE_HPP

#include <string>
#include <folly/coro/Task.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.client.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "streamr-dht/dht/contact/RpcRemote.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/StreamPartID.hpp"

namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::logger::SLogger;
using ContentDeliveryRpcClient =
    streamr::protorpc::ContentDeliveryRpcClient<DhtCallContext>;
using streamr::utils::StreamPartID;
class ContentDeliveryRpcRemote : public RpcRemote<ContentDeliveryRpcClient> {
public:
    ContentDeliveryRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        ContentDeliveryRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<ContentDeliveryRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              client,
              timeout) {}

    folly::coro::Task<void> sendStreamMessage(StreamMessage msg) {
        auto options = this->formDhtRpcOptions({});
        try {
            co_await this->getClient().sendStreamMessage(
                std::move(msg), std::move(options));
        } catch (const std::exception& err) {
            SLogger::trace(
                "Failed to sendStreamMessage: " + std::string(err.what()));
        }
    }

    folly::coro::Task<void> leaveStreamPartNotice(
        const StreamPartID& streamPartId, bool isLocalNodeEntryPoint) {
        LeaveStreamPartNotice notification;
        notification.set_streampartid(streamPartId);
        notification.set_isentrypoint(isLocalNodeEntryPoint);
        auto options = this->formDhtRpcOptions({});
        try {
            co_await this->getClient().leaveStreamPartNotice(
                std::move(notification), std::move(options));
        } catch (const std::exception& err) {
            SLogger::trace(
                "Failed to send leaveStreamPartNotice: " +
                std::string(err.what()));
        }
    }
};

} // namespace streamr::trackerlessnetwork

#endif