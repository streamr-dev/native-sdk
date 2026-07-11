// Module streamr.dht.WebrtcConnectorRpcRemote
// Ported from packages/dht/src/connection/webrtc/WebrtcConnectorRpcRemote.ts
// (v103.8.0-rc.3): the signalling notifications the connector sends to the
// remote peer over the transport. Adaptation: the TS methods fire the
// notification and .catch() the promise; here each method is a coroutine
// the WebrtcConnector spawns detached into its scope — the co_await keeps
// the request locals alive in this frame (the lazy-task trap:
// RpcCommunicatorClientApi::notify holds its parameters by reference), and
// the connector's spawn wrapper logs failures like the TS .catch.
module;

#include <chrono>
#include <optional>
#include <string>

// std::coroutine_traits must be visible in every translation unit
// that defines OR instantiates a coroutine; it cannot arrive through
// an imported BMI.
#include <coroutine> // IWYU pragma: keep

export module streamr.dht.WebrtcConnectorRpcRemote;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.dht.DhtRpcClient;
import streamr.protorpc.RpcCommunicator;
import streamr.logger.SLogger;
import streamr.dht.Connection;
import streamr.dht.DhtCallContext;
import streamr.dht.RpcRemote;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht::connection::webrtc {

using ::dht::IceCandidate;
using ::dht::PeerDescriptor;
using ::dht::RtcAnswer;
using ::dht::RtcOffer;
using ::dht::WebrtcConnectionRequest;
using streamr::dht::connection::ConnectionID;
using streamr::dht::contact::DhtCallContext;
using streamr::dht::contact::RpcRemote;

using WebrtcConnectorRpcClient =
    ::dht::WebrtcConnectorRpcClient<DhtCallContext>;

class WebrtcConnectorRpcRemote : public RpcRemote<WebrtcConnectorRpcClient> {
public:
    WebrtcConnectorRpcRemote(
        PeerDescriptor&& localPeerDescriptor, // NOLINT
        PeerDescriptor&& remotePeerDescriptor,
        WebrtcConnectorRpcClient&& client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<WebrtcConnectorRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              client,
              timeout) {}

    folly::coro::Task<void> requestConnection() {
        WebrtcConnectionRequest request{};
        auto options = this->formDhtRpcOptions();
        co_await this->getClient().requestConnection(
            std::move(request), std::move(options), this->getTimeout());
    }

    folly::coro::Task<void> sendRtcOffer(
        std::string description, ConnectionID connectionId) {
        RtcOffer request;
        request.set_connectionid(connectionId);
        request.set_description(std::move(description));
        auto options = this->formDhtRpcOptions();
        co_await this->getClient().rtcOffer(
            std::move(request), std::move(options), this->getTimeout());
    }

    folly::coro::Task<void> sendRtcAnswer(
        std::string description, ConnectionID connectionId) {
        RtcAnswer request;
        request.set_connectionid(connectionId);
        request.set_description(std::move(description));
        auto options = this->formDhtRpcOptions();
        co_await this->getClient().rtcAnswer(
            std::move(request), std::move(options), this->getTimeout());
    }

    folly::coro::Task<void> sendIceCandidate(
        std::string candidate, std::string mid, ConnectionID connectionId) {
        IceCandidate request;
        request.set_connectionid(connectionId);
        request.set_mid(std::move(mid));
        request.set_candidate(std::move(candidate));
        auto options = this->formDhtRpcOptions();
        co_await this->getClient().iceCandidate(
            std::move(request), std::move(options), this->getTimeout());
    }
};

} // namespace streamr::dht::connection::webrtc
