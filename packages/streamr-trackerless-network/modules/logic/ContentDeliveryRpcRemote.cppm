// Module streamr.trackerlessnetwork.ContentDeliveryRpcRemote
// CONSOLIDATED from the former header logic/ContentDeliveryRpcRemote.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

export module streamr.trackerlessnetwork.ContentDeliveryRpcRemote;

import streamr.trackerlessnetwork.protos;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.RpcRemote;
import streamr.dht.protos;
import streamr.logger.SLogger;
import streamr.utils.StreamPartID;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::logger::SLogger;
using streamr::utils::StreamPartID;
export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using ContentDeliveryRpcClient =
    streamr::protorpc::ContentDeliveryRpcClient<DhtCallContext>;
class ContentDeliveryRpcRemote : public RpcRemote<ContentDeliveryRpcClient> {
private:
    std::optional<int64_t> rtt;

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

    void setRtt(int64_t rttMilliseconds) { this->rtt = rttMilliseconds; }

    [[nodiscard]] std::optional<int64_t> getRtt() const { return this->rtt; }

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
