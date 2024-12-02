#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTOR_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTOR_HPP

#include <functional>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-dht/connection/OutgoingHandshaker.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnectorRpcLocal.hpp"
#include "streamr-dht/helpers/Connectivity.hpp"
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"

namespace streamr::dht::connection::websocket {

using ::dht::ConnectivityMethod;
using ::dht::PeerDescriptor;
using ::dht::WebsocketConnectionRequest;
using streamr::dht::Identifiers;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::websocket::WebsocketClientConnection;
using streamr::dht::helpers::Connectivity;
using streamr::dht::transport::ListeningRpcCommunicator;

struct WebsocketClientConnectorOptions {
    std::function<bool(const std::shared_ptr<IPendingConnection>&)>
        onNewConnection;
    std::function<bool(DhtAddress)> hasConnection;
    ListeningRpcCommunicator& rpcCommunicator;
};

class WebsocketClientConnector {
private:
    PeerDescriptor localPeerDescriptor;
    std::map<DhtAddress, std::shared_ptr<OutgoingHandshaker>>
        connectingHandshakers;
    AbortController abortController;
    WebsocketClientConnectorOptions options;
    WebsocketClientConnectorRpcLocal rpcLocal;
    std::recursive_mutex mutex;

public:
    static constexpr auto websocketConnectorServiceId =
        "system/websocket-connector";

    explicit WebsocketClientConnector(WebsocketClientConnectorOptions&& options)
        : options(std::move(options)),
          rpcLocal(WebsocketClientConnectorRpcLocalOptions{
              .connect = [this](const PeerDescriptor& targetPeerDescriptor)
                  -> std::shared_ptr<IPendingConnection> {
                  return this->connect(targetPeerDescriptor, std::nullopt);
              },
              .hasConnection = [this](const DhtAddress& nodeId) -> bool {
                  std::scoped_lock lock(this->mutex);
                  return this->connectingHandshakers.find(nodeId) !=
                      this->connectingHandshakers.end() ||
                      this->options.hasConnection(nodeId);
              },
              .onNewConnection =
                  [this](
                      const std::shared_ptr<IPendingConnection>& connection) {
                      return this->options.onNewConnection(connection);
                  },
              .abortSignal = this->abortController.getSignal()}) {
        this->options.rpcCommunicator
            .registerRpcNotification<WebsocketConnectionRequest>(
                "requestConnection",
                [this](
                    const WebsocketConnectionRequest& req,
                    const DhtCallContext& context) -> void {
                    std::scoped_lock lock(this->mutex);
                    if (this->abortController.getSignal().aborted) {
                        return;
                    }
                    return this->rpcLocal.requestConnection(req, context);
                });
    }

    bool isPossibleToFormConnection(
        const PeerDescriptor& targetPeerDescriptor) {
        std::scoped_lock lock(this->mutex);
        const auto connectionType = Connectivity::expectedConnectionType(
            this->localPeerDescriptor, targetPeerDescriptor);
        return connectionType == ConnectionType::WEBSOCKET_CLIENT;
    }

    std::shared_ptr<IPendingConnection> connect(
        const PeerDescriptor& targetPeerDescriptor,
        std::optional<std::function<void(std::exception_ptr)>> errorCallback =
            std::nullopt) {
        SLogger::debug("connect() to " + targetPeerDescriptor.DebugString());
        std::scoped_lock lock(this->mutex);
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetPeerDescriptor);
        const auto existingHandshaker =
            this->connectingHandshakers.find(nodeId);
        if (existingHandshaker != this->connectingHandshakers.end()) {
            return existingHandshaker->second->getPendingConnection();
        }

        auto socket = WebsocketClientConnection::newInstance();

        const auto url = Connectivity::connectivityMethodToWebsocketUrl(
            targetPeerDescriptor.websocket());

        auto pendingConnection = std::make_shared<PendingConnection>(
            targetPeerDescriptor, std::move(errorCallback));

        std::shared_ptr<OutgoingHandshaker> outgoingHandshaker =
            OutgoingHandshaker::newInstance(
                this->localPeerDescriptor,
                socket,
                targetPeerDescriptor,
                pendingConnection);

        this->connectingHandshakers.emplace(nodeId, outgoingHandshaker);

        outgoingHandshaker->on<handshakerevents::HandshakerStopped>(
            [this, nodeId]() {
                std::scoped_lock lock(this->mutex);
                if (this->connectingHandshakers.find(nodeId) !=
                    this->connectingHandshakers.end()) {
                    this->connectingHandshakers.erase(nodeId);
                }
            });

        socket->connect(url, false);

        return pendingConnection;
    }

    void setLocalPeerDescriptor(const PeerDescriptor& peerDescriptor) {
        std::scoped_lock lock(this->mutex);
        this->localPeerDescriptor = peerDescriptor;
    }

    void destroy() {
        std::scoped_lock lock(this->mutex);
        this->abortController.abort();

        for (const auto& [_, handshaker] : this->connectingHandshakers) {
            handshaker->getPendingConnection()->close(true);
        }

        this->connectingHandshakers.clear();
    }
};

} // namespace streamr::dht::connection::websocket

#endif // STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETCLIENTCONNECTOR_HPP
