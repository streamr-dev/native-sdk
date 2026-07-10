#include <cstdint>
#include <string>
#include <string_view>
#include <gtest/gtest.h>
#include <rtc/global.hpp>

import streamr.dht.Connection;
import streamr.dht.Transport;
import streamr.dht.WebsocketClientConnection;
import streamr.dht.WebsocketServer;
import streamr.dht.WebsocketServerConnection;
import streamr.logger.SLogger;

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

// Ported from packages/dht/test/integration/Websocket.test.ts
// (v103.8.0-rc.3) 'Happy path': the server sends first as soon as the
// connection is up, the client echoes the payload back, and the server
// asserts the round trip. Adaptations: the C++ connection events do not
// replay to late listeners, so each side attaches its Data listener before
// any send can race it (the TS event loop gives the same ordering for
// free), and the port is unique to this suite instead of the TS 9977.
TEST(WebsocketClientServerTest, TestServerClientRoundTripMessageExchange) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    constexpr uint16_t roundTripPort = 10002;
    WebsocketServerConfig config{
        .portRange = {.min = roundTripPort, .max = roundTripPort},
        .enableTls = false,
        .tlsCertificateFiles = std::nullopt,
        .maxMessageSize = std::nullopt};

    WebsocketServer server(std::move(config));

    const std::vector<std::byte> payload{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};

    std::promise<std::vector<std::byte>> clientReceivedPromise;
    std::promise<std::vector<std::byte>> serverReceivedPromise;

    std::shared_ptr<WebsocketServerConnection> serverConnection;

    server.on<websocketserverevents::Connected>(
        [&](const std::shared_ptr<WebsocketServerConnection>& connection) {
            serverConnection = connection;
            serverConnection->on<Data>(
                [&](const std::vector<std::byte>& message) {
                    serverReceivedPromise.set_value(message);
                });
            serverConnection->send(payload);
        });

    server.start();

    auto client = WebsocketClientConnection::newInstance();
    client->on<Data>([&](const std::vector<std::byte>& message) {
        clientReceivedPromise.set_value(message);
        // Echo the payload back to the server.
        client->send(message);
    });

    client->connect("ws://127.0.0.1:" + std::to_string(roundTripPort), false);

    const auto clientReceived = clientReceivedPromise.get_future().get();
    ASSERT_EQ(clientReceived, payload);
    const auto serverReceived = serverReceivedPromise.get_future().get();
    ASSERT_EQ(serverReceived, payload);

    client->close(false);
    serverConnection->close(false);
    server.stop();
}
