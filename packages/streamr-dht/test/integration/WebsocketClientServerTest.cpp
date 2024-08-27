#include <string_view>
#include <rtc/global.hpp>
#include <streamr-dht/connection/websocket/WebsocketClientConnection.hpp>
#include <streamr-dht/connection/websocket/WebsocketServerConnection.hpp>
#include <streamr-dht/connection/websocket/WebsocketServer.hpp>
#include <streamr-dht/utils/Errors.hpp>
#include <streamr-logger/SLogger.hpp>
#include <gtest/gtest.h>

using streamr::dht::connection::websocket::WebsocketServer;
using streamr::dht::connection::websocket::WebsocketClientConnection;
using streamr::dht::connection::websocket::WebsocketServerConnection;
using streamr::dht::connection::websocket::WebsocketServerConfig;
using streamr::dht::connection::websocket::events::Connected;
using streamr::dht::connection::events::Data;
using streamr::logger::SLogger;

TEST(WebsocketClientServerTest, TestServerCanAcceptConnection) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    WebsocketServerConfig config {
        .portRange = {10001, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles = std::nullopt,
        .maxMessageSize = std::nullopt
    };

    WebsocketServer server(std::move(config));
   
    std::promise<std::string_view> connectionEstablishedPromise;
    server.on<Connected>([&](const std::shared_ptr<WebsocketServerConnection>& /*connection*/) {
        SLogger::trace("in onConnected() event handler");
        connectionEstablishedPromise.set_value("Hello, world!");
    });

    server.start();

    WebsocketClientConnection client;
    client.connect("ws://127.0.0.1:10001", false); // NOLINT

    const std::string_view message = connectionEstablishedPromise.get_future().get();
    SLogger::trace("message: {}", message);
    ASSERT_EQ(message, "Hello, world!");
    
    client.close(false);
    server.stop();
}

TEST(WebsocketClientServerTest, TestServerCanTrasmitMessageToServer) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    WebsocketServerConfig config {
        .portRange = {10001, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles = std::nullopt,
        .maxMessageSize = std::nullopt
    };

    WebsocketServer server(std::move(config));

    std::promise<std::vector<std::byte>> messagePromise;
    
    std::shared_ptr<WebsocketServerConnection> serverConnection;
    
    server.on<Connected>([&](const std::shared_ptr<WebsocketServerConnection>& connection) {
        SLogger::trace("in onConnected() event handler");
        serverConnection = connection;  // make sure the connection does not get destroyed
        serverConnection->on<Data>([&](const std::vector<std::byte>& message) {
            SLogger::trace("in onMessage() event handler");
            messagePromise.set_value(message);
        });
    });

    server.start();

    std::vector<std::byte> message;
    message.push_back(std::byte{1});
    message.push_back(std::byte{2});
    message.push_back(std::byte{3});

    WebsocketClientConnection client;
    
    client.on<streamr::dht::connection::events::Connected>([&]() {
        client.send(message);
    });

    client.connect("ws://127.0.0.1:10001", false); // NOLINT

    auto receivedMessage = messagePromise.get_future().get();
    
    ASSERT_EQ(message, receivedMessage);
    
    client.close(false);
    serverConnection->close(false);
    server.stop();

}
