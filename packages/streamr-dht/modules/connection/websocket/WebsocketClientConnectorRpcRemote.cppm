// Module streamr.dht.WebsocketClientConnectorRpcRemote
// CONSOLIDATED from the former header
// streamr-dht/connection/websocket/WebsocketClientConnectorRpcRemote.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;
#include <chrono>
#include <optional>
#include <string>

// std::coroutine_traits must be visible in every translation unit
// that defines OR instantiates a coroutine; it cannot arrive through
// an imported BMI.
#include <coroutine> // IWYU pragma: keep

export module streamr.dht.WebsocketClientConnectorRpcRemote;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.dht.DhtRpcClient;
import streamr.protorpc.RpcCommunicator;
import streamr.logger.SLogger;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RpcRemote;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
export namespace streamr::dht::connection::websocket {

using ::dht::PeerDescriptor;
using ::dht::WebsocketConnectionRequest;
using streamr::dht::Identifiers;
using streamr::dht::contact::DhtCallContext;
using streamr::dht::contact::RpcRemote;

using RpcCommunicator = streamr::protorpc::RpcCommunicator<DhtCallContext>;
using WebsocketClientConnectorRpcClient =
    ::dht::WebsocketClientConnectorRpcClient<DhtCallContext>;

class WebsocketClientConnectorRpcRemote
    : public RpcRemote<WebsocketClientConnectorRpcClient> {
public:
    WebsocketClientConnectorRpcRemote(
        PeerDescriptor&& localPeerDescriptor, // NOLINT
        PeerDescriptor&& remotePeerDescriptor,
        WebsocketClientConnectorRpcClient&& client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<WebsocketClientConnectorRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              client,
              timeout) {}

    folly::coro::Task<void> requestConnection() {
        SLogger::trace(
            "Requesting WebSocket connection from " +
            Identifiers::getNodeIdFromPeerDescriptor(getLocalPeerDescriptor()));
        WebsocketConnectionRequest request{};
        auto options = this->formDhtRpcOptions();
        return this->getClient().requestConnection(
            std::move(request), std::move(options), this->getTimeout());
    }
};

} // namespace streamr::dht::connection::websocket
