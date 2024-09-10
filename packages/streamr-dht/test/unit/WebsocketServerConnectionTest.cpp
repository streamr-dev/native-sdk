#include "streamr-dht/connection/websocket/WebsocketServerConnection.hpp"
#include <gtest/gtest.h>
#include "rtc/rtc.hpp"

using streamr::dht::connection::websocket::WebsocketServerConnection;

#include <gmock/gmock.h>

TEST(WebsocketServerConnection, TestCanBeCreated) {
    auto connection = WebsocketServerConnection::newInstance();
}
