// Module streamr.dht.WebsocketServerConnection
// CONSOLIDATED from the former header
// streamr-dht/connection/websocket/WebsocketServerConnection.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <rtc/rtc.hpp>

export module streamr.dht.WebsocketServerConnection;

import std;

import streamr.dht.Connection;
import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;
import streamr.dht.WebsocketConnection;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;
export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::Url;
class WebsocketServerConnection : public WebsocketConnection {
private:
    Url mResourceURL;
    std::string mRemoteAddress;
    HandlerToken mHalfReadyConnectedHandlerToken;

    WebsocketServerConnection()
        : WebsocketConnection(ConnectionType::WEBSOCKET_SERVER),
          mResourceURL(Url{""}) {
        this->mHalfReadyConnectedHandlerToken = this->on<
            connectionevents::Connected>([this]() {
            SLogger::trace(
                "WebsocketServerConnection() The half-ready socket is fully connected");
            mResourceURL = Url{mSocket->path().value()};
            mRemoteAddress = mSocket->remoteAddress().value();
            this->off<connectionevents::Connected>(
                this->mHalfReadyConnectedHandlerToken);
        });
    }

public:
    [[nodiscard]] static std::shared_ptr<WebsocketServerConnection>
    newInstance() {
        struct MakeSharedEnabler : public WebsocketServerConnection {};
        return std::make_shared<MakeSharedEnabler>();
    }

    ~WebsocketServerConnection() override {
        SLogger::trace("~WebsocketServerConnection()");
    }

    void setDataChannelWebSocket(std::shared_ptr<rtc::WebSocket> ws) {
        SLogger::trace("WebsocketServerConnection() setting socket");
        setSocket(std::move(ws));
        SLogger::trace("WebsocketServerConnection() setSocket() finished");
    }

    [[nodiscard]] Url getResourceURL() const { return mResourceURL; }

    [[nodiscard]] std::string getRemoteAddress() const {
        return mRemoteAddress;
    }
};

} // namespace streamr::dht::connection::websocket
