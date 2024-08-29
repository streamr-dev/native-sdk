#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_SERVERCONNECTION_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_SERVERCONNECTION_HPP

#include <string>
#include <rtc/rtc.hpp>
#include "streamr-dht/connection/websocket/WebsocketConnection.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::websocket {

using streamr::dht::connection::Url;
using streamr::logger::SLogger;

class WebsocketServerConnection : public WebsocketConnection {
private:
    Url mResourceURL;
    std::string mRemoteAddress;

public:
    WebsocketServerConnection(
        std::shared_ptr<rtc::WebSocket>&& socket,
        Url&& resourceURL,
        std::string_view remoteAddress)
        : WebsocketConnection(ConnectionType::WEBSOCKET_SERVER),
          mResourceURL(std::move(resourceURL)),
          mRemoteAddress(remoteAddress) {
        SLogger::trace("WebsocketServerConnection() setSocket");
        setSocket(std::move(socket));
    }

    ~WebsocketServerConnection() override {
        SLogger::trace("~WebsocketServerConnection()");
    }

    [[nodiscard]] Url getResourceURL() const {
        return mResourceURL;
    }

    [[nodiscard]] std::string getRemoteAddress() const {
        return mRemoteAddress;
    }
};

} // namespace streamr::dht::connection::websocket

#endif // STREAMR_DHT_CONNECTION_WEBSOCKET_SERVERCONNECTION_HPP