#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_CONNECTION_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_CONNECTION_HPP

#include <rtc/rtc.hpp>
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::websocket {

using streamr::logger::SLogger;
using streamr::dht::connection::events::Connected;
using streamr::dht::connection::events::Data;
using streamr::dht::connection::events::Disconnected;
using streamr::dht::connection::events::Error;

constexpr size_t maxMessageSize = 1048576;

class WebsocketConnection : public Connection {
protected:
    std::shared_ptr<rtc::WebSocket> mSocket; // NOLINT
    std::atomic<bool> mDestroyed{false}; // NOLINT
    std::recursive_mutex mMutex; // NOLINT

    // Only allow subclasses to be created
    explicit WebsocketConnection(ConnectionType type) : Connection(type) {}

    void setSocket(std::shared_ptr<rtc::WebSocket>&& socket) {
        std::lock_guard<std::recursive_mutex> lock(mMutex);
        mSocket = std::move(socket);

        // Set socket callbacks

        mSocket->onError([this](const std::string& error) {
            SLogger::trace(
                "onError() " + error,
                {{"connectionType", getConnectionTypeString()},
                 {"mDestroyed", mDestroyed.load()}});
            if (!mDestroyed) {
                emit<Error>(error);
            }
        });

        mSocket->onOpen([this]() {
            SLogger::trace(
                "onOpen()",
                {{"connectionType", getConnectionTypeString()},
                 {"mDestroyed", mDestroyed.load()}});
            if (!mDestroyed) {
                if (mSocket &&
                    mSocket->readyState() == rtc::WebSocket::State::Open) {
                    emit<Connected>();
                }
            }
        });

        mSocket->onClosed([this]() {
            SLogger::trace(
                "onClosed()",
                {
                    {"connectionType", getConnectionTypeString()},
                    {"mDestroyed", mDestroyed.load()},
                });

            if (!mDestroyed) {
                std::lock_guard<std::recursive_mutex> lock(mMutex);
                mDestroyed = true;
                mSocket->resetCallbacks();
                mSocket = nullptr;

                const auto gracefulLeave = false;
                emit<Disconnected>(gracefulLeave, 0, "");
                this->removeAllListeners();
            }
        });

        mSocket->onMessage([this](const std::variant<rtc::binary, std::string>&
                                      message) {
            SLogger::trace(
                "onMessage()",
                {
                    {"connectionType", getConnectionTypeString()},
                    {"mDestroyed", mDestroyed.load()},
                });

            if (!mDestroyed) {
                if (std::holds_alternative<rtc::binary>(message)) {
                    emit<Data>(std::get<rtc::binary>(message));
                } else {
                    SLogger::debug(
                        "onMessage() received string data, only binary data is supported");
                }
            }
        });
    }

public:
    ~WebsocketConnection() override {
        SLogger::trace(
            "~WebsocketConnection()",
            {
                {"connectionType", getConnectionTypeString()},
            });
        destroy();
    };

    void send(const std::vector<std::byte>& data) override {
        SLogger::trace(
            "send()",
            {{"connectionType", getConnectionTypeString()},
             {"mDestroyed", mDestroyed.load()}});
        if (!mDestroyed && mSocket &&
            mSocket->readyState() == rtc::WebSocket::State::Open) {
            mSocket->send(data);
        } else {
            SLogger::debug(
                "send() on non-open connection",
                {{"connectionType", getConnectionTypeString()},
                 {"mDestroyed", mDestroyed.load()}});
            return;
        }
    }

    void close(bool /* gracefulLeave*/) override {
        SLogger::trace(
            "close()",
            {{"connectionType", getConnectionTypeString()},
             {"mDestroyed", mDestroyed.load()}});
        if (!mDestroyed) {
            mSocket->close();
        } else {
            SLogger::debug("close() on destroyed connection");
        }
    }

    void destroy() override {
        SLogger::trace(
            "destroy()",
            {{"connectionType", getConnectionTypeString()},
             {"mDestroyed", mDestroyed.load()}});

        if (mDestroyed) {
            SLogger::debug("destroy() on destroyed connection");
            return;
        }

        mDestroyed = true;

        if (mSocket) {
            std::lock_guard<std::recursive_mutex> lock(mMutex);
            mSocket->forceClose();
            mSocket = nullptr;
        }
    }
};

} // namespace streamr::dht::connection::websocket

#endif