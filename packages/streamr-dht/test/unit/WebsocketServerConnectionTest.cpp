#include "streamr-dht/connection/websocket/WebsocketServerConnection.hpp"
#include "rtc/rtc.hpp"
#include <gtest/gtest.h>

using streamr::dht::connection::websocket::WebsocketServerConnection;

#include <gmock/gmock.h>


TEST(WebsocketServerConnection, TestCanBeCreated) {
    WebsocketServerConnection connection;
}
