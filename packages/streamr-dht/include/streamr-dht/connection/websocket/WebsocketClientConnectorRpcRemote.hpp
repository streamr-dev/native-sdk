#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCREMOTE_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCREMOTE_HPP

#include <folly/experimental/coro/Task.h>
#include "packages/dht/protos/DhtRpc.client.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/dht/contact/RpcRemote.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::websocket {

using ::dht::PeerDescriptor;
using ::dht::WebsocketConnectionRequest;
using streamr::dht::Identifiers;
using streamr::dht::contact::DhtCallContext;
using streamr::dht::contact::RpcRemote;
using streamr::logger::SLogger;

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

#endif
