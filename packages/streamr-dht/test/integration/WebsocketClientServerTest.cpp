#include <string_view>
#include <gtest/gtest.h>
#include <rtc/global.hpp>
#include <streamr-dht/connection/websocket/WebsocketClientConnection.hpp>
#include <streamr-dht/connection/websocket/WebsocketServer.hpp>
#include <streamr-dht/connection/websocket/WebsocketServerConnection.hpp>
#include <streamr-logger/SLogger.hpp>

using streamr::dht::connection::connectionevents::Connected;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::connection::websocket::WebsocketClientConnection;
using streamr::dht::connection::websocket::WebsocketServer;
using streamr::dht::connection::websocket::WebsocketServerConfig;
using streamr::dht::connection::websocket::WebsocketServerConnection;
using streamr::logger::SLogger;

namespace websocketserverevents =
    streamr::dht::connection::websocket::websocketserverevents;

TEST(WebsocketClientServerTest, TestServerCanAcceptConnection) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    WebsocketServerConfig config{
        .portRange = {10001, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles = std::nullopt,
        .maxMessageSize = std::nullopt};

    WebsocketServer server(std::move(config));

    std::promise<std::string> connectionEstablishedPromise;
    server.on<websocketserverevents::Connected>(
        [&](const std::shared_ptr<WebsocketServerConnection>& /*connection*/) {
            SLogger::info("in onConnected() event handler");
            connectionEstablishedPromise.set_value("Hello, world!");
        });
    SLogger::info("before start()");
    server.start();
    SLogger::info("after start()");

    auto client = WebsocketClientConnection::newInstance();
    SLogger::info("before connect()");
    client->connect("ws://127.0.0.1:10001", false); // NOLINT
    SLogger::info("after connect()");
    auto message = connectionEstablishedPromise.get_future().get();
    SLogger::info("message:", message);
    ASSERT_EQ(message, "Hello, world!");
    SLogger::info("before close()");
    client->close(false);
    SLogger::info("after close()");
    SLogger::info("before stop()");
    server.stop();
    SLogger::info("after stop()");
}

TEST(WebsocketClientServerTest, TestClientCanTrasmitMessageToServer) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    WebsocketServerConfig config{
        .portRange = {10001, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles = std::nullopt,
        .maxMessageSize = std::nullopt};

    WebsocketServer server(std::move(config));

    std::promise<std::vector<std::byte>> messagePromise;

    std::shared_ptr<WebsocketServerConnection> serverConnection;

    server.on<websocketserverevents::Connected>(
        [&](const std::shared_ptr<WebsocketServerConnection>& connection) {
            SLogger::trace("in onConnected() event handler");
            serverConnection =
                connection; // make sure the connection does not get destroyed
            serverConnection->on<Data>(
                [&](const std::vector<std::byte>& message) {
                    SLogger::trace("in onMessage() event handler");
                    messagePromise.set_value(message);
                });
        });

    server.start();

    std::vector<std::byte> message;
    message.push_back(std::byte{1});
    message.push_back(std::byte{2});
    message.push_back(std::byte{3});

    auto client = WebsocketClientConnection::newInstance();

    client->on<streamr::dht::connection::connectionevents::Connected>([&]() {
        SLogger::trace("in client onConnected() event handler");
        client->send(message);
    });

    client->connect("ws://127.0.0.1:10001", false); // NOLINT

    auto receivedMessage = messagePromise.get_future().get();

    ASSERT_EQ(message, receivedMessage);

    client->close(false);
    serverConnection->close(false);
    server.stop();
}
