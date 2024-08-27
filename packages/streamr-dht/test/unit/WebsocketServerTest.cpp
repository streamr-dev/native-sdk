#include <streamr-dht/connection/websocket/WebsocketServer.hpp>
#include <streamr-dht/utils/Errors.hpp>
#include <gtest/gtest.h>

using streamr::dht::connection::websocket::WebsocketServer;
using streamr::dht::connection::websocket::WebsocketServerConfig;
using streamr::dht::utils::WebsocketServerStartError;

TEST(WebsocketServerTest, TestStartAndStopServer) {
    WebsocketServerConfig config {
        .portRange = {10000, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles = std::nullopt,
        .maxMessageSize = std::nullopt
    };

    WebsocketServer server(std::move(config));
    server.start();
    server.stop();
}

TEST(WebsocketServerTest, TestServerThrowsIfPortIsInUse) {
    WebsocketServerConfig config {
        .portRange = {10001, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles = std::nullopt,
        .maxMessageSize = std::nullopt
    };

    WebsocketServerConfig config2 = config;

    WebsocketServer server(std::move(config));
    server.start();
    
    WebsocketServer server2(std::move(config2));
    EXPECT_THROW(server2.start(), WebsocketServerStartError);

    server.stop();
}
