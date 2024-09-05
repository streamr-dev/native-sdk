#include <gtest/gtest.h>
#include "streamr-dht/connection/websocket/WebsocketClientConnection.hpp"
#include <streamr-dht/helpers/Errors.hpp>

using streamr::dht::connection::websocket::WebsocketClientConnection;

TEST(WebsocketClientConnection, TestCanBeCreated) {
    WebsocketClientConnection connection;
}

TEST(WebsocketClientConnection, TestInvalidAddress) {
    WebsocketClientConnection connection;
    connection.connect("InvalidAddress", false);
}

TEST(WebsocketClientConnection, TestCannotSendDataIfNoConnection) {
    WebsocketClientConnection connection;
    std::vector<std::byte> message;
    message.push_back(std::byte{1});
    connection.send(message);

}
