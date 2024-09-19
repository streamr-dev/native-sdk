#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCLOCAL_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCLOCAL_HPP

#include <functional>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/dht/protos/DhtRpc.server.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-utils/AbortController.hpp"

namespace streamr::dht::connection::websocket {

using ::dht::PeerDescriptor;
using ::dht::WebsocketClientConnectorRpc;
using ::dht::WebsocketConnectionRequest;
using streamr::dht::DhtAddress;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::utils::AbortSignal;

struct WebsocketClientConnectorRpcLocalOptions {
    std::function<std::shared_ptr<IPendingConnection>(const PeerDescriptor&)>
        connect;
    std::function<bool(const DhtAddress& /*nodeId*/)> hasConnection;
    std::function<bool(const std::shared_ptr<IPendingConnection>&)>
        onNewConnection;
    AbortSignal& abortSignal;
};

class WebsocketClientConnectorRpcLocal
    : public WebsocketClientConnectorRpc<DhtCallContext> {
private:
    WebsocketClientConnectorRpcLocalOptions options;

public:
    explicit WebsocketClientConnectorRpcLocal(
        const WebsocketClientConnectorRpcLocalOptions&& options)
        : options(options) {}
    ~WebsocketClientConnectorRpcLocal() override = default;

    void requestConnection(
        const WebsocketConnectionRequest& /*request*/,
        const DhtCallContext& dhtCallContext) override {
        if (this->options.abortSignal.aborted) {
            return;
        }

        const auto senderPeerDescriptor =
            dhtCallContext.incomingSourceDescriptor.value();
        if (!this->options.hasConnection(
                Identifiers::getNodeIdFromPeerDescriptor(
                    senderPeerDescriptor))) {
            const auto connection = this->options.connect(senderPeerDescriptor);
            this->options.onNewConnection(connection);
        }
    }
};

} // namespace streamr::dht::connection::websocket

#endif // STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCLOCAL_HPP