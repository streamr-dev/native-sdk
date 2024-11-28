#ifndef STREAMR_DHT_CONNECTION_CONNECTORFACADE_HPP
#define STREAMR_DHT_CONNECTION_CONNECTORFACADE_HPP

#include <chrono>
#include <memory>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnector.hpp"
#include "streamr-dht/connection/websocket/WebsocketServerConnector.hpp"
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-dht/types/PortRange.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"

namespace streamr::dht::connection {

using namespace std::chrono_literals;
using ::dht::ConnectivityResponse;
using std::chrono::milliseconds;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::IPendingConnection;
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
    virtual std::shared_ptr<IPendingConnection> createConnection(
        const PeerDescriptor& peerDescriptor) = 0;
    virtual std::shared_ptr<IPendingConnection> createConnection(
        const PeerDescriptor& peerDescriptor,
        std::function<void(std::exception_ptr)> errorCallback) = 0;
    [[nodiscard]] virtual PeerDescriptor getLocalPeerDescriptor() const = 0;
    virtual void start(
        std::function<bool(const std::shared_ptr<IPendingConnection>&)> onNewConnection,
        std::function<bool(const DhtAddress& nodeId)> hasConnection /*,
        Transport& autoCertifierTransport*/ ) = 0;
    virtual void stop() = 0;
};

struct DefaultConnectorFacadeOptions {
    Transport& transport;
    std::optional<std::string> websocketHost = std::nullopt;
    std::optional<PortRange> websocketPortRange = std::nullopt;
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

    ~DefaultConnectorFacade() {
        SLogger::info("DefaultConnectorFacade::~DefaultConnectorFacade");
    }

    void start(
        std::function<bool(const std::shared_ptr<IPendingConnection>&)>
            onNewConnection,
        std::function<bool(const DhtAddress& nodeId)> hasConnection /*,
        Transport& autoCertifierTransport */
        ) override {
        SLogger::info("DefaultConnectorFacade::start() start");
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
            .portRange = this->options.websocketPortRange,
            .maxMessageSize = this->options.maxMessageSize,
            .host = this->options.websocketHost,
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

        if (this->websocketServerConnector) {
            SLogger::debug(
                "DefaultConnectorFacade::start() websocketServerConnector is not null");
        } else {
            SLogger::debug(
                "DefaultConnectorFacade::start() websocketServerConnector is null");
        }
        SLogger::info("DefaultConnectorFacade::start() end");
    }

    std::shared_ptr<IPendingConnection> createConnection(
        const PeerDescriptor& peerDescriptor) override {
        if (this->websocketClientConnector->isPossibleToFormConnection(
                peerDescriptor)) {
            return this->websocketClientConnector->connect(peerDescriptor);
        }

        return nullptr;
    }

    std::shared_ptr<IPendingConnection> createConnection(
        const PeerDescriptor& peerDescriptor,
        std::function<void(std::exception_ptr)> errorCallback) override {
        if (this->websocketClientConnector->isPossibleToFormConnection(
                peerDescriptor)) {
            return this->websocketClientConnector->connect(
                peerDescriptor, std::move(errorCallback));
        }

        return nullptr;
    }

    PeerDescriptor getLocalPeerDescriptor() const override {
        return this->localPeerDescriptor;
    }

    void stop() override {
        SLogger::info("DefaultConnectorFacade::stop start");
        this->websocketConnectorRpcCommunicator.destroy();
        this->websocketClientConnector->destroy();
        this->websocketServerConnector->destroy();
        SLogger::info("DefaultConnectorFacade::stop end");
    }
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_CONNECTORFACADE_HPP
