// Module streamr.dht.WebsocketClientConnectorRpcLocal
// CONSOLIDATED from the former header
// streamr-dht/connection/websocket/WebsocketClientConnectorRpcLocal.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;



export module streamr.dht.WebsocketClientConnectorRpcLocal;

import std;

import streamr.dht.protos;

import streamr.dht.DhtRpcServer;
import streamr.utils.AbortController;
import streamr.dht.DhtCallContext;
import streamr.dht.IPendingConnection;
import streamr.dht.Identifiers;
import streamr.dht.PendingConnection;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::utils::AbortSignal;
export namespace streamr::dht::connection::websocket {

using ::dht::PeerDescriptor;
using ::dht::WebsocketClientConnectorRpc;
using ::dht::WebsocketConnectionRequest;
using streamr::dht::DhtAddress;
using streamr::dht::rpcprotocol::DhtCallContext;

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
