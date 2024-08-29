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
#include "streamr-dht/utils/CertificateHelper.hpp"
#include "streamr-dht/utils/Errors.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::websocket {

using streamr::dht::connection::events::Data;
using streamr::dht::connection::events::Disconnected;
using streamr::dht::connection::events::Error;
using streamr::dht::connection::Url;
using streamr::dht::utils::CertificateHelper;
using streamr::dht::utils::TlsCertificate;
using streamr::dht::utils::WebsocketServerStartError;
using streamr::logger::SLogger;

struct PortRange {
    uint16_t min;
    uint16_t max;
};

struct TlsCertificateFiles {
    std::string privateKeyFileName;
    std::string certFileName;
};

struct WebsocketServerConfig {
    PortRange portRange;
    bool enableTls;
    std::optional<TlsCertificateFiles> tlsCertificateFiles;
    std::optional<size_t> maxMessageSize;
};

namespace events {

struct Connected : Event<std::shared_ptr<WebsocketServerConnection>> {};

} // namespace events

using WebsocketServerEvents = std::tuple<events::Connected>;

constexpr size_t defaultMaxMessageSize = 1048576;

class WebsocketServer : public EventEmitter<WebsocketServerEvents> {
private:
    std::shared_ptr<rtc::WebSocketServer> mServer;
    WebsocketServerConfig mConfig;
    uint16_t mPort;
    bool mTls;
    std::unordered_map<std::string, std::shared_ptr<rtc::WebSocket>>
        mHalfReadySockets;

public:
    explicit WebsocketServer(WebsocketServerConfig&& config)
        : mConfig(std::move(config)) {}
    // uint16_t start() {}

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
        SLogger::trace("stop()");
        removeAllListeners();
        mServer->stop();
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

    void handleHalfReadySocket(const std::shared_ptr<rtc::WebSocket> ws) {
        auto id = boost::uuids::to_string(boost::uuids::random_generator()());
        mHalfReadySockets.insert({id, ws});

        ws->onOpen([id, this]() {
            SLogger::info("Hafl-Ready WebSocket client connected");
            const auto& ws = mHalfReadySockets[id];
            if (!ws->path().has_value() || !ws->remoteAddress().has_value()) {
                return;
            }
            auto parsedUrl = Url{ws->path().value()};
            auto parsedRemoteAddress = ws->remoteAddress().value().substr(
                0, ws->remoteAddress().value().find(':'));
            emit<events::Connected>(std::make_shared<WebsocketServerConnection>(
                std::move(mHalfReadySockets[id]),
                std::move(parsedUrl),
                parsedRemoteAddress));
            SLogger::info(
                "handleHalfReadySocket. Before erase");
            mHalfReadySockets.erase(id);
        });

        ws->onClosed([id, this]() {
            SLogger::info(
                "WebSocket client disconnected, calling forceClose()");
            const auto& it = mHalfReadySockets.find(id);  
            if (it == mHalfReadySockets.end()) { 
                // Already erased
                return;
            } 
            mHalfReadySockets.erase(it);
        });

        ws->onError([id, this](const std::string& error) {
            SLogger::trace("WebSocket Client error: " + error);
            mHalfReadySockets.erase(id);
        });
    }

    void startServer(
        uint16_t port, bool tls, std::optional<TlsCertificate> certs) {
        mPort = port;
        mTls = tls;

        rtc::WebSocketServerConfiguration webSocketServerConfiguration{
            .port = port,
            .enableTls = false,
            .bindAddress = "0.0.0.0",
            .maxMessageSize =
                mConfig.maxMessageSize.value_or(defaultMaxMessageSize)};

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

        mServer->onClient([&](std::shared_ptr<rtc::WebSocket>&& ws) {
            SLogger::trace("onClient()");

            if (!ws->path().has_value() || !ws->remoteAddress().has_value()) {
                SLogger::trace(
                    "onClient() !(ws->path().has_value() || !ws->remoteAddress().has_value())");
                handleHalfReadySocket(ws);
            } else {
                SLogger::trace(
                    "onClient() ws->path().has_value() && ws->remoteAddress().has_value()");
                SLogger::trace(
                    "onClient() trying to parse url of length: " +
                    std::to_string(ws->path().value().length()));

                auto parsedUrl = Url{ws->path().value()};

                SLogger::trace("onClient() parsedUrl: " + parsedUrl);

                auto parsedRemoteAddress = ws->remoteAddress().value().substr(
                    0, ws->remoteAddress().value().find(':'));

                SLogger::trace(
                    "onClient() parsedRemoteAddress: " + parsedRemoteAddress);
                SLogger::trace("onClient() trying to emit<Connected>()");

                emit<events::Connected>(std::make_shared<WebsocketServerConnection>(
                    std::move(ws), std::move(parsedUrl), parsedRemoteAddress));
            }
        });
    }
};

} // namespace streamr::dht::connection::websocket

#endif // STREAMR_DHT_CONNECTION_WEBSOCKET_WEBSERVER_HPP