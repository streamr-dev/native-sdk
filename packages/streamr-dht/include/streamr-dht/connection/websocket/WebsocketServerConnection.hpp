#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_SERVERCONNECTION_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_SERVERCONNECTION_HPP

#include <string>
#include <rtc/rtc.hpp>
#include "streamr-dht/connection/websocket/WebsocketConnection.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::websocket {

using streamr::dht::connection::Url;
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;
class WebsocketServerConnection : public WebsocketConnection {
private:
    Url mResourceURL;
    std::string mRemoteAddress;
    HandlerToken mHalfReadyConnectedHandlerToken;

public:
    WebsocketServerConnection()
        : WebsocketConnection(ConnectionType::WEBSOCKET_SERVER),
          mResourceURL(Url{""}) {
        this->mHalfReadyConnectedHandlerToken = this->on<
            connectionevents::Connected>([this]() {
            SLogger::trace(
                "WebsocketServerConnection() The half-ready socket is fully connected");
            mResourceURL = Url{mSocket->path().value()};
            mRemoteAddress = mSocket->remoteAddress().value();
            this->off<connectionevents::Connected>(this->mHalfReadyConnectedHandlerToken);
        });
    }

    ~WebsocketServerConnection() override {
        SLogger::trace("~WebsocketServerConnection()");
    }

    void setDataChannelWebSocket(std::shared_ptr<rtc::WebSocket> ws) {
        SLogger::trace("WebsocketServerConnection() setting socket");
        setSocket(ws);
        SLogger::trace(
            "WebsocketServerConnection() setSocket() finished");
    }

    [[nodiscard]] Url getResourceURL() const { return mResourceURL; }

    [[nodiscard]] std::string getRemoteAddress() const {
        return mRemoteAddress;
    }
};

} // namespace streamr::dht::connection::websocket

#endif // STREAMR_DHT_CONNECTION_WEBSOCKET_SERVERCONNECTION_HPP