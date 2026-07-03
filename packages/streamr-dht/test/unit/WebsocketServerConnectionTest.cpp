#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "rtc/rtc.hpp"

import streamr.dht;

using streamr::dht::connection::websocket::WebsocketServerConnection;

TEST(WebsocketServerConnection, TestCanBeCreated) {
    auto connection = WebsocketServerConnection::newInstance();
}
