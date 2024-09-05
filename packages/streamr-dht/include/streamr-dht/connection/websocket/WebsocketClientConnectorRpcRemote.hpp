#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCREMOTE_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCREMOTE_HPP

#include "streamr-dht/Identifiers.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-dht/dht/contact/RpcRemote.hpp"
#include "packages/dht/protos/DhtRpc.client.pb.h"
#include <folly/coro/Task.h>

namespace streamr::dht::connection::websocket {

using streamr::logger::SLogger;
using streamr::dht::contact::RpcRemote;
using streamr::dht::Identifiers;
using ::dht::WebsocketClientConnectorRpcClient;
using ::dht::WebsocketConnectionRequest;
using ::dht::PeerDescriptor;

class WebsocketClientConnectorRpcRemote : public RpcRemote<WebsocketClientConnectorRpcClient> {
public:
    WebsocketClientConnectorRpcRemote(
        const PeerDescriptor& localPeerDescriptor,  // NOLINT
        const PeerDescriptor& remotePeerDescriptor,
        WebsocketClientConnectorRpcClient& client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<WebsocketClientConnectorRpcClient>(localPeerDescriptor, remotePeerDescriptor, client, timeout) {
    }

    folly::coro::Task<void> requestConnection() {
        SLogger::trace("Requesting WebSocket connection from " + Identifiers::getNodeIdFromPeerDescriptor(
            getLocalPeerDescriptor()));
        WebsocketConnectionRequest request{};
        auto options = this->formDhtRpcOptions();
        return this->getClient().requestConnection(request, options);
    }
};

} // namespace streamr::dht::connection::websocket

#endif
