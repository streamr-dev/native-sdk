#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_CLIENTWEBSOCKET_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_CLIENTWEBSOCKET_HPP

#include <rtc/rtc.hpp>
#include "streamr-dht/connection/websocket/WebsocketConnection.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::websocket {

using streamr::logger::SLogger;

class WebsocketClientConnection : public WebsocketConnection {
public:
    explicit WebsocketClientConnection() : WebsocketConnection(ConnectionType::WEBSOCKET_CLIENT) {}
        
    ~WebsocketClientConnection() override {
        SLogger::trace("~WebsocketClientConnection()");
    }

    void connect(const std::string& address, bool selfSigned = true) {
        std::lock_guard<std::recursive_mutex> lock(mMutex);
        if (mDestroyed) {
            SLogger::debug("connect() on destroyed connection");
            return;
        }

        rtc::WebSocketConfiguration webSocketConfig{
            .disableTlsVerification = selfSigned,
            .maxMessageSize = maxMessageSize};

        setSocket(std::make_unique<rtc::WebSocket>(webSocketConfig));
        SLogger::trace("socket created");
        mSocket->open(address);
        SLogger::trace("connect() mSocket->open() called");
    }
};

} // namespace streamr::dht::connection::websocket

#endif