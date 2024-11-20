#include "streamr-dht/connection/websocket/WebsocketClientConnection.hpp"
#include <gtest/gtest.h>
#include <streamr-dht/helpers/Errors.hpp>

using streamr::dht::connection::websocket::WebsocketClientConnection;

TEST(WebsocketClientConnection, TestCanBeCreated) {
    auto connection = WebsocketClientConnection::newInstance();
}

TEST(WebsocketClientConnection, TestInvalidAddress) {
    auto connection = WebsocketClientConnection::newInstance();
    connection->connect("InvalidAddress", false);
}

TEST(WebsocketClientConnection, TestCannotSendDataIfNoConnection) {
    auto connection = WebsocketClientConnection::newInstance();
    std::vector<std::byte> message;
    message.push_back(std::byte{1});
    connection->send(message);
}
