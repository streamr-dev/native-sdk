// Module streamr.dht.WebsocketClientConnection
// CONSOLIDATED from the former header
// streamr-dht/connection/websocket/WebsocketClientConnection.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

// std::coroutine_traits must be visible in every translation unit
// that defines OR instantiates a coroutine; it cannot arrive through
// an imported BMI.

#include <rtc/rtc.hpp>


export module streamr.dht.WebsocketClientConnection;

import std;

import streamr.utils.CoroutineHelper;
import streamr.dht.Connection;
import streamr.logger.SLogger;
import streamr.utils.waitForEvent;
import streamr.dht.WebsocketConnection;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
using streamr::utils::waitForEvent;
export namespace streamr::dht::connection::websocket {

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
        std::scoped_lock lock(mMutex);
        if (mDestroyed) {
            SLogger::debug("connect() on destroyed connection");
            return;
        }

        rtc::WebSocketConfiguration webSocketConfig{
            .disableTlsVerification = selfSigned,
            .maxMessageSize = maxMessageSize};

        auto socket = std::make_shared<rtc::WebSocket>(webSocketConfig);
        setSocket(socket);
        // The client's listeners are registered before connect(), so the
        // message callback can be attached right away — before open(), as
        // it always was (setSocket() no longer attaches it itself).
        startReceiving();
        SLogger::trace("socket created");
        socket->open(address);
        SLogger::trace("connect() mSocket->open() called");
    }
};

} // namespace streamr::dht::connection::websocket
