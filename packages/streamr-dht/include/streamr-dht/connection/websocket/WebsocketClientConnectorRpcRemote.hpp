#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCREMOTE_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCREMOTE_HPP

#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-dht/dht/contact/RpcRemote.hpp"
#include "packages/dht/protos/DhtRpc.client.pb.h"
#include <folly/coro/Task.h>

namespace streamr::dht::connection::websocket {

using streamr::logger::SLogger;
using streamr::dht::contact::RpcRemote;
using streamr::dht::Identifiers;
using ::dht::WebsocketConnectionRequest;
using ::dht::PeerDescriptor;
using streamr::dht::contact::DhtCallContext;

using RpcCommunicator = streamr::protorpc::RpcCommunicator<DhtCallContext>;
using WebsocketClientConnectorRpcClient = ::dht::WebsocketClientConnectorRpcClient<DhtCallContext>; 

class WebsocketClientConnectorRpcRemote : public RpcRemote<WebsocketClientConnectorRpcClient> {
public:
    WebsocketClientConnectorRpcRemote(
        PeerDescriptor&& localPeerDescriptor,  // NOLINT
        PeerDescriptor&& remotePeerDescriptor,
        WebsocketClientConnectorRpcClient&& client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<WebsocketClientConnectorRpcClient>(std::move(localPeerDescriptor), std::move(remotePeerDescriptor), client, timeout) {
    }

    folly::coro::Task<void> requestConnection() {
        SLogger::trace("Requesting WebSocket connection from " + Identifiers::getNodeIdFromPeerDescriptor(
            getLocalPeerDescriptor()));
        WebsocketConnectionRequest request {};
        auto options = this->formDhtRpcOptions();
        return this->getClient().requestConnection(std::move(request), std::move(options), this->getTimeout());
    }
};

} // namespace streamr::dht::connection::websocket

#endif
