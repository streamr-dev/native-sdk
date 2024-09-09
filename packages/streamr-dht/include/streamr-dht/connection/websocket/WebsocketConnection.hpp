#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_CONNECTION_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_CONNECTION_HPP

#include <rtc/rtc.hpp>
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection::websocket {

using streamr::dht::connection::connectionevents::Connected;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::connection::connectionevents::Disconnected;
using streamr::dht::connection::connectionevents::Error;
using streamr::logger::SLogger;

constexpr size_t maxMessageSize = 1048576;

class WebsocketConnection : public Connection, public std::enable_shared_from_this<WebsocketConnection> {
protected:
    std::shared_ptr<rtc::WebSocket> mSocket; // NOLINT
    std::atomic<bool> mDestroyed{false}; // NOLINT
    std::recursive_mutex mMutex; // NOLINT

    // Only allow subclasses to be created
    explicit WebsocketConnection(ConnectionType type) : Connection(type) {}

    void setSocket(std::shared_ptr<rtc::WebSocket> socket) {
        SLogger::trace("setSocket() " + getConnectionTypeString());
        SLogger::debug("Trying to acquire mutex lock in setSocket");
        std::lock_guard<std::recursive_mutex> lock(mMutex);
        SLogger::debug("Acquired mutex lock in setSocket");
        mSocket = socket;

        SLogger::trace("setSocket() after move " + getConnectionTypeString());
        // Set socket callbacks

        mSocket->onMessage(
            [this](rtc::binary data) {
                SLogger::trace(
                    "onMessage()",
                    {
                        {"connectionType", getConnectionTypeString()},
                        {"mDestroyed", mDestroyed.load()},
                    });
                auto self = shared_from_this();
                if (!mDestroyed) {
                    if (!data.empty()) {
                        SLogger::trace(
                            "onMessage() received binary data",
                            {{"size", data.size()}});
                        emit<Data>(data);
                    } else {
                        SLogger::debug(
                            "onMessage() received empty binary data from binary-only callback");
                    }
                }
            },
            [this](const std::string&) {
                SLogger::debug(
                    "onMessage() received string data, only binary data is supported");
            });

        mSocket->onError([this](const std::string& error) {
            SLogger::trace(
                "onError() " + error,
                {{"connectionType", getConnectionTypeString()},
                 {"mDestroyed", mDestroyed.load()}});
            if (!mDestroyed) {
                auto self = shared_from_this();
                emit<Error>(error);
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
                auto self = shared_from_this();
                SLogger::debug("Trying to acquire mutex lock in onClosed");
                std::lock_guard<std::recursive_mutex> lock(mMutex);
                SLogger::debug("Acquired mutex lock in onClosed");
                mDestroyed = true;
                mSocket->resetCallbacks();
                mSocket = nullptr;

                const auto gracefulLeave = false;
                emit<Disconnected>(gracefulLeave, 0, "");
                this->removeAllListeners();
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
                    auto self = shared_from_this();
                    SLogger::trace(
                        "onOpen() emitting Connected " +
                            getConnectionTypeString(),
                        {{"numberofListeners", listenerCount<Connected>()}});

                    emit<Connected>();
                }
            }
        });

        SLogger::trace("setSocket() end " + getConnectionTypeString());
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
             {"mDestroyed", mDestroyed.load()},
             {"size", data.size()}});
        if (!mDestroyed && mSocket &&
            mSocket->readyState() == rtc::WebSocket::State::Open) {
            auto dataCopy = data;
            SLogger::trace(
                "send() sending data",
                {{"connectionType", getConnectionTypeString()},
                 {"mDestroyed", mDestroyed.load()},
                 {"size", data.size()}});
            mSocket->send(std::move(dataCopy));
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
            mSocket->resetCallbacks();
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
            SLogger::debug("destroy() trying to get mutex lock");
            std::lock_guard<std::recursive_mutex> lock(mMutex);
            SLogger::debug("destroy() got mutex lock");
            mSocket->forceClose();
            mSocket = nullptr;
        }
    }
};

} // namespace streamr::dht::connection::websocket

#endif