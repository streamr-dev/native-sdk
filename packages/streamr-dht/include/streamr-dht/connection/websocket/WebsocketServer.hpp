#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSERVER_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSERVER_HPP

#include <cstdint>
#include <fstream>
#include <ranges>
#include <sstream>
#include <string>
#include <unordered_map>
#include <rtc/rtc.hpp>
#include "streamr-dht/connection/websocket/WebsocketServerConnection.hpp"
#include "streamr-dht/helpers/CertificateHelper.hpp"
#include "streamr-dht/helpers/Errors.hpp"
#include "streamr-dht/types/PortRange.hpp"
#include "streamr-dht/types/TlsCertificateFiles.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/Uuid.hpp"

namespace streamr::dht::connection::websocket {

using streamr::dht::connection::Url;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::connection::connectionevents::Disconnected;
using streamr::dht::connection::connectionevents::Error;
using streamr::dht::helpers::CertificateHelper;
using streamr::dht::helpers::TlsCertificate;
using streamr::dht::helpers::WebsocketServerStartError;
using streamr::dht::types::PortRange;
using streamr::dht::types::TlsCertificateFiles;
using streamr::logger::SLogger;
using streamr::utils::Uuid;

struct WebsocketServerConfig {
    PortRange portRange;
    bool enableTls;
    std::optional<TlsCertificateFiles> tlsCertificateFiles;
    std::optional<size_t> maxMessageSize;
};

namespace websocketserverevents {

struct Connected : Event<std::shared_ptr<WebsocketServerConnection>> {};

} // namespace websocketserverevents

using WebsocketServerEvents = std::tuple<websocketserverevents::Connected>;

constexpr size_t defaultMaxMessageSize = 1048576;

class WebsocketServer : public EventEmitter<WebsocketServerEvents> {
private:
    std::shared_ptr<rtc::WebSocketServer> mServer;
    WebsocketServerConfig mConfig;
    uint16_t mPort;
    bool mTls;
    std::unordered_map<std::string, std::shared_ptr<WebsocketServerConnection>>
        mHalfReadyConnections;
    std::recursive_mutex mHalfReadyConnectionsMutex;
    std::atomic<bool> mStopped = false;

public:
    explicit WebsocketServer(WebsocketServerConfig&& config)
        : mConfig(std::move(config)) {}

    ~WebsocketServer() override {
        SLogger::trace("~WebsocketServer() start");
        stop();
        SLogger::trace("~WebsocketServer() stop() end");
    }

    uint16_t start() {
        uint32_t min = mConfig.portRange.min;
        uint32_t max = mConfig.portRange.max + 1;
        for (const uint16_t port : std::views::iota(min, max)) {
            try {
                startServer(port, mConfig.enableTls, std::nullopt);
                return port;
            } catch (const WebsocketServerStartError& err) {
                if (err.originalErrorInfo.has_value() &&
                    err.originalErrorInfo.value().find(
                        "TCP server socket binding failed") !=
                        std::string::npos) {
                    SLogger::warn(
                        "failed to start WebSocket server on port: " +
                        std::to_string(port) + " reattempting on next port");
                } else {
                    throw err;
                }
            }
        }
        throw WebsocketServerStartError(
            "Failed to start WebSocket server on any port in range: " +
            std::to_string(mConfig.portRange.min) + "-" +
            std::to_string(mConfig.portRange.max));
    }

    void stop() {
        SLogger::info("stop() start");
        if (mStopped) {
            SLogger::info("stop() already stopped, returning");
            return;
        }
        mStopped = true;
        removeAllListeners();
        SLogger::info(
            "stop() removeAllListeners() end, calling mServer->stop()");
        mServer->stop();
        SLogger::info("stop() mServer->stop() end");
    }

    void updateCertificate(const std::string& cert, const std::string& key) {
        SLogger::trace("Updating WebSocket server certificate");
        mServer->stop();
        startServer(
            mPort, mTls, TlsCertificate{.privateKey = key, .cert = cert});
    }

private:
    // This is needed because libDatachannel gives us the sockets in onClient()
    // in an unpredicatable state. We need to keep them in a map until they
    // are fully ready, otherwise they would get destroyed when the smart
    // pointer falls out of scope.

    void handleIncomingClient(std::shared_ptr<rtc::WebSocket> ws) {
        std::scoped_lock lock(mHalfReadyConnectionsMutex);
        auto websocketServerConnection =
            WebsocketServerConnection::newInstance();
        auto id = Uuid::v4();
        mHalfReadyConnections.insert({id, websocketServerConnection});

        websocketServerConnection->on<Connected>([this, id]() {
            SLogger::info(
                "Half-ready WebSocketServerConnection emitted Connected, emitting it as Connected");
            std::scoped_lock lock(mHalfReadyConnectionsMutex);
            auto readyConnection = mHalfReadyConnections.at(id);

            readyConnection->removeAllListeners();

            emit<websocketserverevents::Connected>(readyConnection);
            SLogger::info("handleHalfReadySocket. Before erase");
            mHalfReadyConnections.erase(id);
        });

        websocketServerConnection->on<
            Disconnected>([id, this](
                              bool /* gracefulLeave */,
                              uint64_t /* code */,
                              const std::string& /* reason */) {
            SLogger::info(
                "Half-ready WebSocketServerConnection emitted Disconnected event, erasing it");
            std::scoped_lock lock(mHalfReadyConnectionsMutex);
            const auto& it = mHalfReadyConnections.find(id);
            if (it == mHalfReadyConnections.end()) {
                // Already erased
                return;
            }
            it->second->removeAllListeners();
            mHalfReadyConnections.erase(it);
        });

        websocketServerConnection->on<Error>([id,
                                              this](const std::string& error) {
            SLogger::trace(
                "Half-ready WebSocketServerConnection emitted Error event, erasing it, error: " +
                error);
            std::scoped_lock lock(mHalfReadyConnectionsMutex);
            const auto& it = mHalfReadyConnections.find(id);
            if (it == mHalfReadyConnections.end()) {
                // Already erased
                return;
            }
            it->second->removeAllListeners();
            mHalfReadyConnections.erase(it);
        });

        websocketServerConnection->setDataChannelWebSocket(std::move(ws));
    }

    void startServer(
        uint16_t port, bool tls, std::optional<TlsCertificate> certs) {
        mPort = port;
        mTls = tls;

        rtc::WebSocketServerConfiguration webSocketServerConfiguration{
            .port = port,
            .enableTls = false,
            .bindAddress = "0.0.0.0",
            .maxMessageSize = defaultMaxMessageSize};

        if (mConfig.maxMessageSize.has_value() &&
            mConfig.maxMessageSize.value() > 0) {
            webSocketServerConfiguration.maxMessageSize =
                mConfig.maxMessageSize.value();
        }

        if (certs || mConfig.tlsCertificateFiles || tls) {
            webSocketServerConfiguration.enableTls = true;
        }

        if (certs) {
            webSocketServerConfiguration.certificatePemFile =
                certs.value().cert;
            webSocketServerConfiguration.keyPemFile = certs.value().privateKey;
        } else if (mConfig.tlsCertificateFiles) {
            std::ifstream certFile(mConfig.tlsCertificateFiles->certFileName);
            std::stringstream certBuffer;
            certBuffer << certFile.rdbuf();
            webSocketServerConfiguration.certificatePemFile = certBuffer.str();

            std::ifstream keyFile(
                mConfig.tlsCertificateFiles->privateKeyFileName);
            std::stringstream keyBuffer;
            keyBuffer << keyFile.rdbuf();
            webSocketServerConfiguration.keyPemFile = keyBuffer.str();
        } else if (tls) {
            auto certificate =
                CertificateHelper::createSelfSignedCertificate(1000); // NOLINT
            webSocketServerConfiguration.certificatePemFile = certificate.cert;
            webSocketServerConfiguration.keyPemFile = certificate.privateKey;
        }

        try {
            SLogger::trace(
                "Starting WebSocket server on port " + std::to_string(port));
            mServer = std::make_shared<rtc::WebSocketServer>(
                webSocketServerConfiguration);
        } catch (std::exception& e) {
            throw(WebsocketServerStartError(
                "Starting Websocket server failed", e.what()));
        }
        SLogger::trace(
            "WebSocket server started on port " + std::to_string(port));

        mServer->onClient([&](std::shared_ptr<rtc::WebSocket> ws) {
            SLogger::trace("onClient()");

            handleIncomingClient(std::move(ws));
        });
    }
};

} // namespace streamr::dht::connection::websocket

#endif // STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSERVER_HPP