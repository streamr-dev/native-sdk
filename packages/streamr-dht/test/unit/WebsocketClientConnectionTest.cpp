#include <gtest/gtest.h>
#include "streamr-dht/connection/websocket/WebsocketClientConnection.hpp"

using streamr::dht::connection::websocket::WebsocketClientConnection;

TEST(WebsocketClientConnection, TestCanBeCreated) {
    WebsocketClientConnection connection;
}
