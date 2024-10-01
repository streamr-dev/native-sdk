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
        std::scoped_lock lock(mMutex);
        SLogger::debug("Acquired mutex lock in setSocket");
        mSocket = std::move(socket);

        SLogger::trace("setSocket() after move " + getConnectionTypeString());
        // Set socket callbacks

        mSocket->onMessage(
            [this](rtc::binary data) {
                //std::scoped_lock lock(this->mMutex);
                SLogger::trace(
                    "onMessage()",
                    {
                        {"connectionType", getConnectionTypeString()},
                        {"mDestroyed", mDestroyed.load()},
                    });
                auto self = this->sharedFromThis<WebsocketConnection>();
                if (!self->mDestroyed) {
                    if (!data.empty()) {
                        SLogger::trace(
                            "onMessage() received binary data",
                            {{"size", data.size()}});
                        self->emit<Data>(std::move(data));
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
                 {"mDestroyed", this->mDestroyed.load()}});
            if (!this->mDestroyed) {
                auto self = this->sharedFromThis<WebsocketConnection>();
                self->emit<Error>(error);
            }
        });

        mSocket->onClosed([this]() {
            std::scoped_lock lock(this->mMutex);
            SLogger::trace(
                "onClosed()",
                {
                    {"connectionType", getConnectionTypeString()},
                    {"mDestroyed", this->mDestroyed.load()},
                });

            if (!this->mDestroyed) {
                //SLogger::debug("Trying to acquire mutex lock in onClosed " + getConnectionTypeString());
                //std::scoped_lock lock(mMutex);
                SLogger::debug("Getting shared pointer in onClosed");
                auto self = this->sharedFromThis<Connection>();
                SLogger::debug("Got shared pointer in onClosed");
                SLogger::debug("Setting mDestroyed to true in onClosed");
                this->mDestroyed = true;
                
                
                
                //SLogger::debug("Acquired mutex lock in onClosed " + getConnectionTypeString());
                //SLogger::debug("Resetting callbacks in onClosed " + getConnectionTypeString());
                //self->mSocket->resetCallbacks();
                /*
                if (this->mSocket) { 
                    SLogger::debug("Resetting socket in onClosed");
                    this->mSocket.reset();
                } else {
                    SLogger::debug("Socket is nullptr in onClosed");
                }
                */
                this->mSocket->close();
                
                SLogger::debug("Emitting Disconnected in onClosed " + getConnectionTypeString());
                //const auto gracefulLeave = false;
                this->emit<Disconnected>(false, 0, "");
                SLogger::debug("Removing all listeners in onClosed" + getConnectionTypeString());
                this->removeAllListeners();
            }
        });

        mSocket->onOpen([this]() {
            std::scoped_lock lock(this->mMutex);
            auto self = this->sharedFromThis<WebsocketConnection>();
            SLogger::trace(
                "onOpen()",
                {{"connectionType", getConnectionTypeString()},
                 {"mDestroyed", mDestroyed.load()}});
            if (!self->mDestroyed) {
                if (self->mSocket &&
                    self->mSocket->readyState() == rtc::WebSocket::State::Open) {
                    SLogger::trace(
                        "onOpen() emitting Connected " +
                            getConnectionTypeString(),
                        {{"numberofListeners", listenerCount<Connected>()}});

                    self->emit<Connected>();
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
            "send() start",
            {{"connectionType", getConnectionTypeString()},
             {"mDestroyed", mDestroyed.load()},
             {"size", data.size()}});
        auto self = this->sharedFromThis<WebsocketConnection>();
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
        SLogger::trace("send() end");
    }

    void close(bool /* gracefulLeave*/) override {
        SLogger::trace(
            "close()",
            {{"connectionType", getConnectionTypeString()},
             {"mDestroyed", mDestroyed.load()}});
        SLogger::debug("close() trying to acquire mutex lock in close()" + getConnectionTypeString());
        std::scoped_lock lock(mMutex);
        SLogger::debug("close() got mutex lock in close()" + getConnectionTypeString());   
        if (!mDestroyed) {
            //SLogger::debug("close() resetting callbacks");
            //mSocket->resetCallbacks();
            mSocket->close();
            //mSocket = nullptr;
        } else {
            SLogger::debug("close() on destroyed connection");
        }
    }

    void destroy() override {
        SLogger::trace(
            "destroy()",
            {{"connectionType", getConnectionTypeString()},
             {"mDestroyed", mDestroyed.load()}});
        SLogger::debug("destroy() trying to acquire mutex lock");
        std::scoped_lock lock(mMutex);
        SLogger::debug("destroy() got mutex lock");
        if (mDestroyed) {
            SLogger::debug("destroy() on destroyed connection");
            return;
        }

        mDestroyed = true;
        if (mSocket) {
              SLogger::debug("destroy() trying to get mutex lock");
               std::lock_guard<std::recursive_mutex> lock(mMutex);
               SLogger::debug("destroy() got mutex lock");
            //mSocket->resetCallbacks();
            mSocket->close();
            //mSocket = nullptr;
        }
    }
};

} // namespace streamr::dht::connection::websocket

#endif