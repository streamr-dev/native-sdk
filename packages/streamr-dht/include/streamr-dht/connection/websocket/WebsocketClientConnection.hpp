#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_CLIENTWEBSOCKET_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_CLIENTWEBSOCKET_HPP

#include <rtc/rtc.hpp>
#include <folly/coro/Collect.h>
#include "streamr-dht/connection/websocket/WebsocketConnection.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/waitForEvent.hpp"

namespace streamr::dht::connection::websocket {

using streamr::logger::SLogger;
using streamr::utils::waitForEvent;
class WebsocketClientConnection : public WebsocketConnection {
private:
    // Only allow creation of this class via newInstance()
    WebsocketClientConnection()
        : WebsocketConnection(ConnectionType::WEBSOCKET_CLIENT) {}

public:
    [[nodiscard]] static std::shared_ptr<WebsocketClientConnection>
    newInstance() {
        struct MakeSharedEnabler : public WebsocketClientConnection {};
        return std::make_shared<MakeSharedEnabler>();
    }

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

        auto socket = std::make_shared<rtc::WebSocket>(webSocketConfig);
        setSocket(socket);
        SLogger::trace("socket created");
        socket->open(address);
        SLogger::trace("connect() mSocket->open() called");
    }
};

} // namespace streamr::dht::connection::websocket

#endif