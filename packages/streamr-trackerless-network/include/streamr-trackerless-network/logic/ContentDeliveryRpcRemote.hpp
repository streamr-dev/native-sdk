#ifndef STREAMR_TRACKERLESS_NETWORK_CONTENT_DELIVERY_RPC_REMOTE_HPP
#define STREAMR_TRACKERLESS_NETWORK_CONTENT_DELIVERY_RPC_REMOTE_HPP

#include <string>
#include "streamr-logger/SLogger.hpp"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "packages/network/protos/NetworkRpc.client.pb.h"
#include "streamr-dht/dht/contact/RpcRemote.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include <folly/coro/Task.h>

namespace streamr::trackerlessnetwork {

using streamr::logger::SLogger;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using ContentDeliveryRpcClient = streamr::protorpc::ContentDeliveryRpcClient<DhtCallContext>;

class ContentDeliveryRpcRemote : public RpcRemote<ContentDeliveryRpcClient> {

    folly::coro::Task<void> sendStreamMessage(StreamMessage&& msg) {
        auto options = this->formDhtRpcOptions({
        });
        try {
            co_await this->getClient().sendStreamMessage(std::move(msg), std::move(options));
        } catch (const std::exception& err) {
            SLogger::trace("Failed to sendStreamMessage: " + std::string(err.what()));
        }
    }

    folly::coro::Task<void> leaveStreamPartNotice(const StreamPartID& streamPartId, bool isLocalNodeEntryPoint) {
        LeaveStreamPartNotice notification;
        notification.set_streampartid(streamPartId);
        notification.set_isentrypoint(isLocalNodeEntryPoint);
        auto options = this->formDhtRpcOptions({});
        try {
            co_await this->getClient().leaveStreamPartNotice(std::move(notification), std::move(options));
        } catch (const std::exception& err) {
            SLogger::trace("Failed to send leaveStreamPartNotice: " + std::string(err.what()));
        }
    }
};


} // namespace streamr::trackerlessnetwork

#endif