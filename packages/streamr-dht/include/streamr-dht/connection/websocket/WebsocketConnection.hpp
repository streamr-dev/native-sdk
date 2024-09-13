#ifndef STREAMR_DHT_CONNECTION_WEBSOCKET_CONNECTION_HPP
#define STREAMR_DHT_CONNECTION_WEBSOCKET_CONNECTION_HPP

#include <rtc/rtc.hpp>
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/EnableSharedFromThis.hpp"

namespace streamr::dht::connection::websocket {

using streamr::dht::connection::connectionevents::Connected;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::connection::connectionevents::Disconnected;
using streamr::dht::connection::connectionevents::Error;
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;

constexpr size_t maxMessageSize = 1048576;

class WebsocketConnection : public Connection, public EnableSharedFromThis {
protected:
    std::shared_ptr<rtc::WebSocket> mSocket; // NOLINT
    std::atomic<bool> mDestroyed{false}; // NOLINT
    std::recursive_mutex mMutex; // NOLINT

    // Only allow subclasses to be created
    explicit WebsocketConnection(ConnectionType type) : Connection(type) {}

    void setSocket(std::shared_ptr<rtc::WebSocket> socket) {
        SLogger::trace("setSocket() " + getConnectionTypeString());
        SLogger::debug("Trying to acquire mutex lock in setSocket");
        mSocket = std::move(socket);
        SLogger::trace("setSocket() after move " + getConnectionTypeString());
        // Set socket callbacks
        mSocket->onMessage(
            [this](rtc::binary data) {
                std::scoped_lock lock(this->mMutex);
                SLogger::trace(
                    "onMessage()",
                    {
                        {"connectionType", getConnectionTypeString()},
                        {"mDestroyed", mDestroyed.load()},
                    });
                auto self = sharedFromThis<WebsocketConnection>();
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
            std::scoped_lock lock(this->mMutex);
            SLogger::trace(
                "onError() " + error,
                {{"connectionType", getConnectionTypeString()},
                 {"mDestroyed", mDestroyed.load()}});
            if (!mDestroyed) {
                auto self = sharedFromThis<WebsocketConnection>();
                emit<Error>(error);
            }
        });

        mSocket->onClosed([this]() {
            std::scoped_lock lock(this->mMutex);
            SLogger::trace(
                "onClosed()",
                {
                    {"connectionType", getConnectionTypeString()},
                    {"mDestroyed", mDestroyed.load()},
                });

            if (!mDestroyed) {
                auto self = sharedFromThis<WebsocketConnection>();
                mDestroyed = true;
                mSocket->resetCallbacks();
                mSocket = nullptr;

                const auto gracefulLeave = false;
                emit<Disconnected>(gracefulLeave, 0, "");
                this->removeAllListeners();
            }
        });

        mSocket->onOpen([this]() {
            std::scoped_lock lock(this->mMutex);
            auto self = sharedFromThis<WebsocketConnection>();
            SLogger::trace(
                "onOpen()",
                {{"connectionType", getConnectionTypeString()},
                 {"mDestroyed", mDestroyed.load()}});
            if (!mDestroyed) {
                if (mSocket &&
                    mSocket->readyState() == rtc::WebSocket::State::Open) {
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
        if (!mDestroyed && mSocket) { 
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
                mSocket->forceClose();
                mSocket = nullptr;
            }
        }
    }
};

} // namespace streamr::dht::connection::websocket

#endif