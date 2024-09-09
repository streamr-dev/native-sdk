#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTOR_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTOR_HPP

#include <functional>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/OutgoingHandshaker.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnectorRpcLocal.hpp"
#include "streamr-dht/helpers/Connectivity.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"

namespace streamr::dht::connection::websocket {

using ::dht::ConnectivityMethod;
using ::dht::PeerDescriptor;
using ::dht::WebsocketConnectionRequest;
using streamr::dht::Identifiers;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::websocket::WebsocketClientConnection;
using streamr::dht::helpers::Connectivity;
using streamr::dht::transport::ListeningRpcCommunicator;

struct WebsocketClientConnectorOptions {
    std::function<bool(const std::shared_ptr<PendingConnection>&)> onNewConnection;
    std::function<bool(DhtAddress)> hasConnection;
    ListeningRpcCommunicator& rpcCommunicator;
};

class WebsocketClientConnector {
private:
    PeerDescriptor localPeerDescriptor;
    std::map<DhtAddress, std::shared_ptr<OutgoingHandshaker>> connectingHandshakers;
    AbortController abortController;
    WebsocketClientConnectorOptions options;
    WebsocketClientConnectorRpcLocal rpcLocal;

public:
    static constexpr auto websocketConnectorServiceId =
        "system/websocket-connector";

    explicit WebsocketClientConnector(WebsocketClientConnectorOptions&& options)
        : options(std::move(options)),
          rpcLocal(WebsocketClientConnectorRpcLocalOptions{
              .connect =
                  [this](const PeerDescriptor& targetPeerDescriptor) {
                      return this->connect(targetPeerDescriptor);
                  },
              .hasConnection = [this](const DhtAddress& nodeId) -> bool {
                  return this->connectingHandshakers.find(nodeId) !=
                      this->connectingHandshakers.end() ||
                      this->options.hasConnection(nodeId);
              },
              .onNewConnection =
                  [this](const std::shared_ptr<PendingConnection>& connection) {
                      return this->options.onNewConnection(connection);
                  },
              .abortSignal = this->abortController.signal}) {
        this->options.rpcCommunicator
            .registerRpcNotification<WebsocketConnectionRequest>(
                "requestConnection",
                [this](
                    const WebsocketConnectionRequest& req,
                    const DhtCallContext& context) -> void {
                    if (this->abortController.signal.aborted) {
                        return;
                    }
                    return this->rpcLocal.requestConnection(req, context);
                });
    }

    static std::string connectivityMethodToWebsocketUrl(
        const ConnectivityMethod& ws,
        const std::optional<std::string>& action = std::nullopt) {
        return (ws.tls() ? "wss://" : "ws://") + ws.host() + ":" +
            std::to_string(ws.port()) +
            (action.has_value() ? "?action=" + action.value() : "");
    }

    bool isPossibleToFormConnection(
        const PeerDescriptor& targetPeerDescriptor) {
        const auto connectionType = Connectivity::expectedConnectionType(
            this->localPeerDescriptor, targetPeerDescriptor);
        return connectionType == ConnectionType::WEBSOCKET_CLIENT;
    }

    std::shared_ptr<PendingConnection> connect(const PeerDescriptor& targetPeerDescriptor) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetPeerDescriptor);
        const auto existingHandshaker =
        this->connectingHandshakers.find(nodeId);
        if (existingHandshaker != this->connectingHandshakers.end()) {
            return existingHandshaker->second->getPendingConnection();
        }
        
        auto socket = std::make_shared<WebsocketClientConnection>();

        const auto url =
            connectivityMethodToWebsocketUrl(targetPeerDescriptor.websocket());

        auto pendingConnection = std::make_shared<PendingConnection>(
            targetPeerDescriptor);

        std::shared_ptr<OutgoingHandshaker> outgoingHandshaker =
            std::make_shared<OutgoingHandshaker>(
                this->localPeerDescriptor,
                socket,
                targetPeerDescriptor,
                std::move(pendingConnection)
                );

        this->connectingHandshakers.emplace(nodeId, outgoingHandshaker);
 
        outgoingHandshaker->on<handshakerevents::HandshakerStopped>([this, nodeId]() {
            if (this->connectingHandshakers.find(nodeId) !=
                this->connectingHandshakers.end()) {
                this->connectingHandshakers.erase(nodeId);
            }
        });

        socket->connect(url, false);

        return pendingConnection;
    }

    void setLocalPeerDescriptor(const PeerDescriptor& peerDescriptor) {
        this->localPeerDescriptor = peerDescriptor;
    }

    void destroy() {
        this->abortController.abort();

        for (const auto& [_, handshaker] : this->connectingHandshakers) {
            handshaker->getPendingConnection()->close(true);
        }
        
        this->connectingHandshakers.clear();
    }
};

} // namespace streamr::dht::connection::websocket

#endif // STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTOR_HPP