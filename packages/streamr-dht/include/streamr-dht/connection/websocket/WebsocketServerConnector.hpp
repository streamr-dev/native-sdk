#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETSERVERCONNECTOR_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSOCKETSERVERCONNECTOR_HPP

#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/Handshaker.hpp"
#include "streamr-dht/connection/IncomingHandshaker.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketServer.hpp"
#include "streamr-dht/helpers/Version.hpp"
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-dht/types/NatType.hpp"
#include "streamr-dht/types/PortRange.hpp"
#include "streamr-dht/types/TlsCertificateFiles.hpp"
#include "streamr-utils/AbortController.hpp"
#include "streamr-utils/Ipv4Helper.hpp"

namespace streamr::dht::connection::websocket {

using ::dht::ConnectivityResponse;
using streamr::dht::connection::IncomingHandshaker;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::websocket::WebsocketServer;
using streamr::dht::connection::websocket::WebsocketServerConnection;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::dht::types::PortRange;
using streamr::dht::types::TlsCertificateFiles;
using streamr::utils::AbortController;
using streamr::utils::Ipv4Helper;
namespace NatType = streamr::dht::types::NatType;

struct WebsocketServerConnectorOptions {
    std::function<bool(const std::shared_ptr<PendingConnection>&)>
        onNewConnection;
    ListeningRpcCommunicator& rpcCommunicator;
    std::function<bool(DhtAddress)> hasConnection;
    std::optional<PortRange> portRange = std::nullopt;
    std::optional<size_t> maxMessageSize = std::nullopt;
    std::optional<std::string> host = std::nullopt;
    std::optional<std::vector<PeerDescriptor>> entrypoints = std::nullopt;
    std::optional<TlsCertificateFiles> tlsCertificateFiles = std::nullopt;
    std::optional<Transport*> autoCertifierTransport = std::nullopt;
    std::optional<std::string> autoCertifierUrl = std::nullopt;
    std::optional<std::string> autoCertifierConfigFile = std::nullopt;
    std::optional<bool> serverEnableTls = std::nullopt;
    std::optional<std::string> geoIpDatabaseFolder = std::nullopt;
};

class WebsocketServerConnector {
private:
    WebsocketServerConnectorOptions options;
    std::unique_ptr<WebsocketServer> websocketServer;
    std::optional<std::string> host;
    std::optional<PeerDescriptor> localPeerDescriptor;
    AbortController abortController;
    std::optional<uint16_t> selectedPort;
    std::map<std::string, std::shared_ptr<IncomingHandshaker>>
        connectingHandshakers;
    std::map<DhtAddress, std::shared_ptr<PendingConnection>>
        ongoingConnectRequests;
    std::recursive_mutex mMutex;

public:
    explicit WebsocketServerConnector(WebsocketServerConnectorOptions&& options)
        : host(options.host), options(std::move(options)) {
        if (this->options.portRange.has_value()) {
            this->websocketServer = std::make_unique<WebsocketServer>(
                std::move(WebsocketServerConfig{
                    .portRange = this->options.portRange.value(),
                    .enableTls = this->options.serverEnableTls.value_or(false),
                    .tlsCertificateFiles = this->options.tlsCertificateFiles,
                    .maxMessageSize = this->options.maxMessageSize}));
        }
    }
    ~WebsocketServerConnector() {
        SLogger::info("WebsocketServerConnector::~WebsocketServerConnector");
    }

    static std::string getActionFromUrl(const Url& resourceUrl) {
        std::string action;
        size_t queryPos = resourceUrl.find("?");
        if (queryPos != std::string::npos) {
            std::string query = resourceUrl.substr(queryPos + 1);
            size_t actionPos = query.find("action=");
            if (actionPos != std::string::npos) {
                size_t valueStart = actionPos + 7; // NOLINT length of "action="
                size_t valueEnd = query.find('&', valueStart);
                if (valueEnd == std::string::npos) {
                    valueEnd = query.length();
                }
                action = query.substr(valueStart, valueEnd - valueStart);
            }
        }
        return action;
    }

    void start() {
        if (!this->abortController.getSignal().aborted &&
            this->websocketServer) {
            this->websocketServer->on<
                websocketserverevents::
                    Connected>([this](const std::shared_ptr<
                                      WebsocketServerConnection>&
                                          serverSocket) {
                const auto resourceUrl = serverSocket->getResourceURL();
                const auto action = getActionFromUrl(resourceUrl);
                SLogger::trace(
                    "WebSocket client connected",
                    {{"action", action},
                     {"remoteAddress", serverSocket->getRemoteAddress()}});

                if (action == "connectivityRequest") {
                    SLogger::trace(
                        "Connectivity request received, not implemented in native-sdk yet");
                    // attachConnectivityRequestHandler(
                    //     serverSocket, this.geoIpLocator)
                } else if (action == "connectivityProbe") {
                    SLogger::trace(
                        "Connectivity probe received, not implemented in native-sdk yet");
                    // no-op
                } else {
                    // The localPeerDescriptor can be undefined here as the
                    // WS server is used for connectivity checks before the
                    // localPeerDescriptor is set during start. Handshaked
                    // connections should be rejected before the
                    // localPeerDescriptor is set.

                    if (this->localPeerDescriptor.has_value()) {
                        SLogger::trace("Server attaching handshaker");
                        this->attachHandshaker(serverSocket);
                    } else {
                        SLogger::trace(
                            "Incoming Websocket connection before localPeerDescriptor was set, closing connection");
                        try {
                            serverSocket->close(false);
                        } catch (...) {
                            SLogger::error(
                                "Error closing incoming Websocket connection before localPeerDescriptor was set");
                        }
                    }
                }
            });
            this->selectedPort = this->websocketServer->start();
        }
    }

    ConnectivityResponse checkConnectivity(
        bool /* allowSelfSignedCertificate */) {
        std::scoped_lock lock(this->mMutex);

        ConnectivityResponse response;

        // if already aborted or websocket server not started
        if (this->abortController.getSignal().aborted ||
            !this->selectedPort.has_value()) {
            response.set_host("127.0.0.1");
            response.set_nattype(NatType::UNKNOWN);
            response.set_ipaddress(Ipv4Helper::ipv4ToNumber("127.0.0.1"));
            response.set_version(Version::localProtocolVersion);
        } else {
            // return connectivity info given in options

            response.set_host(this->host.value());
            response.set_nattype(NatType::OPEN_INTERNET);
            response.set_ipaddress(Ipv4Helper::ipv4ToNumber("127.0.0.1"));
            response.set_version(Version::localProtocolVersion);
            response.mutable_websocket()->set_host(this->host.value());
            response.mutable_websocket()->set_port(this->selectedPort.value());
            response.mutable_websocket()->set_tls(
                this->options.tlsCertificateFiles.has_value());
        }

        return response;
    }

    void setLocalPeerDescriptor(const PeerDescriptor& localPeerDescriptor) {
        std::scoped_lock lock(this->mMutex);
        this->localPeerDescriptor = localPeerDescriptor;
    }

    void destroy() {
        std::scoped_lock lock(this->mMutex);
        this->abortController.abort();

        for (const auto& request : this->ongoingConnectRequests) {
            request.second->close(true);
        }
        for (const auto& handshaker : this->connectingHandshakers) {
            handshaker.second->getPendingConnection()->close(true);
        }
        this->connectingHandshakers.clear();
        this->ongoingConnectRequests.clear();

        if (this->websocketServer != nullptr) {
            this->websocketServer->stop();
        }
    }

private:
    void attachHandshaker(
        const std::shared_ptr<WebsocketServerConnection>& serverSocket) {
        std::scoped_lock lock(this->mMutex);
        const auto handshakerId = Uuid::v4();

        auto handshaker = IncomingHandshaker::newInstance(
            this->localPeerDescriptor.value(),
            serverSocket,
            [this, handshakerId](const DhtAddress& nodeId)
                -> std::shared_ptr<PendingConnection> {
                std::scoped_lock lock(this->mMutex);

                if (this->ongoingConnectRequests.contains(nodeId)) {
                    return this->ongoingConnectRequests.at(nodeId);
                }

                return nullptr;
            },
            this->options.onNewConnection);

        this->connectingHandshakers.emplace(
            handshakerId, std::move(handshaker));

        this->connectingHandshakers.at(handshakerId)
            ->on<handshakerevents::HandshakerStopped>([this, handshakerId]() {
                std::scoped_lock lock(this->mMutex);
                if (this->connectingHandshakers.contains(handshakerId)) {
                    this->connectingHandshakers.erase(handshakerId);
                }
            });
    }
};

} // namespace streamr::dht::connection::websocket
#endif