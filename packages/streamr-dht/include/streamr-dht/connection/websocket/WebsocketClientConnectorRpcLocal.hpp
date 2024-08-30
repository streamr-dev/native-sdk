/*
import { ServerCallContext } from '@protobuf-ts/runtime-rpc'
import {
    PeerDescriptor,
    WebsocketConnectionRequest
} from '../../proto/packages/dht/protos/DhtRpc'
import { IWebsocketClientConnectorRpc } from '../../proto/packages/dht/protos/DhtRpc.server'
import { DhtCallContext } from '../../rpc-protocol/DhtCallContext'
import { Empty } from '../../proto/google/protobuf/empty'
import { getNodeIdFromPeerDescriptor, DhtAddress } from '../../identifiers'
import { PendingConnection } from '../PendingConnection'

interface WebsocketClientConnectorRpcLocalOptions {
    connect: (targetPeerDescriptor: PeerDescriptor) => PendingConnection
    hasConnection: (nodeId: DhtAddress) => boolean
    onNewConnection: (connection: PendingConnection) => boolean
    abortSignal: AbortSignal
}

export class WebsocketClientConnectorRpcLocal implements IWebsocketClientConnectorRpc {

    private readonly options: WebsocketClientConnectorRpcLocalOptions

    constructor(options: WebsocketClientConnectorRpcLocalOptions) {
        this.options = options
    }

    public async requestConnection(_request: WebsocketConnectionRequest, context: ServerCallContext): Promise<Empty> {
        if (this.options.abortSignal.aborted) {
            return {}
        }
        const senderPeerDescriptor = (context as DhtCallContext).incomingSourceDescriptor!
        if (!this.options.hasConnection(getNodeIdFromPeerDescriptor(senderPeerDescriptor))) {
            const connection = this.options.connect(senderPeerDescriptor)
            this.options.onNewConnection(connection)
        }
        return {}
    }
}
*/

#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCLOCAL_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCLOCAL_HPP

#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include <functional>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/dht/protos/DhtRpc.server.pb.h"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/Identifiers.hpp"

namespace streamr::dht::connection::websocket {

using streamr::protorpc::ProtoCallContext;
using streamr::dht::DhtAddress;
using ::dht::PeerDescriptor;
using ::dht::WebsocketClientConnectorRpc;
using ::dht::WebsocketConnectionRequest;

struct WebsocketClientConnectorRpcLocalOptions {
    std::function<PendingConnection(const PeerDescriptor&)> connect;
    std::function<bool(const DhtAddress& /*nodeId*/)> hasConnection;
    std::function<bool(const PendingConnection&)> onNewConnection;
};

class WebsocketClientConnectorRpcLocal : public WebsocketClientConnectorRpc {
public:
    explicit WebsocketClientConnectorRpcLocal(const WebsocketClientConnectorRpcLocalOptions&& options) : options(options) {}
    ~WebsocketClientConnectorRpcLocal() override = default;

    void requestConnection(const WebsocketConnectionRequest& request, const ProtoCallContext& context) override {

    }

private:
    WebsocketClientConnectorRpcLocalOptions options;
};

} // namespace streamr::dht::connection::websocket

#endif // STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTORRPCLOCAL_HPP