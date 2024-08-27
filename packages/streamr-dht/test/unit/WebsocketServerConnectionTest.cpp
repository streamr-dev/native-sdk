#include "streamr-dht/connection/websocket/WebsocketServerConnection.hpp"
#include "rtc/rtc.hpp"
#include <gtest/gtest.h>

using streamr::dht::connection::websocket::WebsocketServerConnection;

#include <gmock/gmock.h>


TEST(WebsocketServerConnection, TestCanBeCreated) {
    auto mockSocket = std::make_shared<rtc::WebSocket>();
    streamr::dht::connection::Url resourceURL("ws://example.com/resource");
    std::string remoteAddress = "192.168.0.1";
    WebsocketServerConnection connection(std::move(mockSocket), std::move(resourceURL), remoteAddress);
}
