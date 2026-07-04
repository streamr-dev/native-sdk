#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "rtc/rtc.hpp"

import streamr.dht.WebsocketServerConnection;

using streamr::dht::connection::websocket::WebsocketServerConnection;

TEST(WebsocketServerConnection, TestCanBeCreated) {
    auto connection = WebsocketServerConnection::newInstance();
}
