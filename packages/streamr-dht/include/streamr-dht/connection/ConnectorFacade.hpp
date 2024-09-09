#ifndef STREAMR_DHT_CONNECTION_CONNECTORFACADE_HPP
#define STREAMR_DHT_CONNECTION_CONNECTORFACADE_HPP

#include <chrono>
#include <memory>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnector.hpp"
#include "streamr-dht/connection/websocket/WebsocketServerConnector.hpp"
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-dht/types/PortRange.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"

namespace streamr::dht::connection {

using namespace std::chrono_literals;
using ::dht::ConnectivityResponse;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::websocket::WebsocketClientConnector;
using streamr::dht::connection::websocket::WebsocketClientConnectorOptions;
using streamr::dht::connection::websocket::WebsocketServerConnector;
using streamr::dht::connection::websocket::WebsocketServerConnectorOptions;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::dht::types::PortRange;
using streamr::protorpc::RpcCommunicatorOptions;

class ConnectorFacade {
protected:
    ConnectorFacade() = default;

public:
    virtual std::shared_ptr<PendingConnection> createConnection(
        const PeerDescriptor& peerDescriptor) = 0;
    [[nodiscard]] virtual PeerDescriptor getLocalPeerDescriptor() const = 0;
    virtual void start(
        std::function<bool(const std::shared_ptr<PendingConnection>&)> onNewConnection,
        std::function<bool(const DhtAddress& nodeId)> hasConnection /*,
        Transport& autoCertifierTransport*/ ) = 0;
    virtual void stop() = 0;
};

struct DefaultConnectorFacadeOptions {
    Transport& transport;
    std::optional<std::string> websocketHost;
    PortRange websocketPortRange;
    // std::vector<PeerDescriptor> entryPoints;
    // std::vector<IceServer> iceServers;
    // bool webrtcAllowPrivateAddresses;
    // int webrtcDatachannelBufferThresholdLow;
    // int webrtcDatachannelBufferThresholdHigh;
    // std::optional<std::string> externalIp;
    // PortRange webrtcPortRange;
    std::optional<size_t> maxMessageSize;
    // TlsCertificate tlsCertificate;
    // bool websocketServerEnableTls;
    // std::optional<std::string> autoCertifierUrl;
    // std::optional<std::string> autoCertifierConfigFile;
    // std::optional<std::string> geoIpDatabaseFolder;

    std::function<PeerDescriptor(ConnectivityResponse)>
        createLocalPeerDescriptor;
};

class DefaultConnectorFacade : public ConnectorFacade {
private:
    DefaultConnectorFacadeOptions options;

    PeerDescriptor localPeerDescriptor;

    ListeningRpcCommunicator websocketConnectorRpcCommunicator;
    std::unique_ptr<WebsocketClientConnector> websocketClientConnector;
    std::unique_ptr<WebsocketServerConnector> websocketServerConnector;

    void setLocalPeerDescriptor(const PeerDescriptor& peerDescriptor) {
        this->localPeerDescriptor = peerDescriptor;
        this->websocketClientConnector->setLocalPeerDescriptor(peerDescriptor);
        this->websocketServerConnector->setLocalPeerDescriptor(peerDescriptor);
    }

public:
    explicit DefaultConnectorFacade(DefaultConnectorFacadeOptions options)
        : options(options),
          websocketConnectorRpcCommunicator(
              ServiceID{WebsocketClientConnector::websocketConnectorServiceId},
              options.transport,
              RpcCommunicatorOptions{.rpcRequestTimeout = 15000ms}) {} // NOLINT

    void start(
        std::function<bool(const std::shared_ptr<PendingConnection>&)> onNewConnection,
        std::function<bool(const DhtAddress& nodeId)> hasConnection /*,
        Transport& autoCertifierTransport */
        ) override {
        WebsocketClientConnectorOptions webSocketClientConnectorOptions{
            .onNewConnection = onNewConnection,
            .hasConnection = hasConnection,
            .rpcCommunicator = websocketConnectorRpcCommunicator};
        this->websocketClientConnector =
            std::make_unique<WebsocketClientConnector>(
                std::move(webSocketClientConnectorOptions));

        WebsocketServerConnectorOptions webSocketServerConnectorOptions{
            .onNewConnection = onNewConnection,
            .rpcCommunicator = websocketConnectorRpcCommunicator,
            .hasConnection = hasConnection,
            .portRange = options.websocketPortRange,
            .maxMessageSize = options.maxMessageSize,
            .host = options.websocketHost,
            //.entrypoints = options.entryPoints,
            //.tlsCertificate = options.tlsCertificate,
            .serverEnableTls = false,
            //.autoCertifierUrl = options.autoCertifierUrl,
            //.autoCertifierConfigFile = options.autoCertifierConfigFile,
            //.autoCertifierTransport = options.autoCertifierTransport,
            //.geoIpDatabaseFolder = options.geoIpDatabaseFolder
        };

        this->websocketServerConnector =
            std::make_unique<WebsocketServerConnector>(
                std::move(webSocketServerConnectorOptions));

        this->websocketServerConnector->start();
        const auto connectivityResponse =
            this->websocketServerConnector->checkConnectivity(false);
        const auto localPeerDescriptor =
            options.createLocalPeerDescriptor(connectivityResponse);
        this->setLocalPeerDescriptor(localPeerDescriptor);
    }

    std::shared_ptr<PendingConnection> createConnection(
        const PeerDescriptor& peerDescriptor) override {
        if (this->websocketClientConnector->isPossibleToFormConnection(
                peerDescriptor)) {
            return this->websocketClientConnector->connect(peerDescriptor);
        }

        return nullptr;
    }

    PeerDescriptor getLocalPeerDescriptor() const override {
        return this->localPeerDescriptor;
    }

    void stop() override {
        this->websocketConnectorRpcCommunicator.destroy();
        this->websocketClientConnector->destroy();
        this->websocketServerConnector->destroy();
    }
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_CONNECTORFACADE_HPP
