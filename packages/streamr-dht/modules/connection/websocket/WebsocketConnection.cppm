// Module streamr.dht.WebsocketConnection
// CONSOLIDATED from the former header
// streamr-dht/connection/websocket/WebsocketConnection.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;
#include <new>

#include <rtc/rtc.hpp>


export module streamr.dht.WebsocketConnection;

import std;

import streamr.logger.SLogger;
import streamr.utils.EnableSharedFromThis;
import streamr.dht.Connection;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;
export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::connectionevents::Connected;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::connection::connectionevents::Disconnected;
using streamr::dht::connection::connectionevents::Error;

inline constexpr std::size_t maxMessageSize = 1048576;

class WebsocketConnection : public Connection, public EnableSharedFromThis {
private:
    std::function<void(const std::string&)> onError;
    std::function<void(rtc::binary)> onMessage;
    std::function<void(const std::string&)> onStringMessage;
    std::function<void()> onOpen;
    std::function<void()> onClosed;

protected:
    std::shared_ptr<rtc::WebSocket> mSocket; // NOLINT
    std::atomic<bool> mDestroyed{false}; // NOLINT
    std::atomic<bool> mConnectedEmitted{false}; // NOLINT
    std::recursive_mutex mMutex; // NOLINT

    // Only allow subclasses to be created
    explicit WebsocketConnection(ConnectionType type)
        : Connection(type),
          onError{[this](const std::string& error) {
              std::scoped_lock lock(this->mMutex);
              SLogger::trace(
                  "onError() " + error,
                  {{"connectionType", getConnectionTypeString()},
                   {"mDestroyed", this->mDestroyed.load()}});
              if (!this->mDestroyed) {
                  auto self = this->sharedFromThis<WebsocketConnection>();
                  self->emit<Error>(error);
              }
          }},
          onMessage{[this](rtc::binary data) {
              // std::scoped_lock lock(this->mMutex);
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
          }},
          onStringMessage{[this](const std::string&) {
              SLogger::debug(
                  "onMessage() received string data, only binary data is supported");
          }},
          onClosed{[this]() {
              std::scoped_lock lock(this->mMutex);
              SLogger::trace(
                  "onClosed()",
                  {
                      {"connectionType", getConnectionTypeString()},
                      {"mDestroyed", this->mDestroyed.load()},
                  });

              if (!this->mDestroyed) {
                  // SLogger::debug("Trying to acquire mutex lock in onClosed "
                  // + getConnectionTypeString()); std::scoped_lock
                  // lock(mMutex);
                  SLogger::debug("Getting shared pointer in onClosed");
                  auto self = this->sharedFromThis<Connection>();
                  SLogger::debug("Got shared pointer in onClosed");
                  SLogger::debug("Setting mDestroyed to true in onClosed");
                  this->mDestroyed = true;

                  // SLogger::debug("Acquired mutex lock in onClosed " +
                  // getConnectionTypeString()); SLogger::debug("Resetting
                  // callbacks in onClosed " + getConnectionTypeString());
                  // self->mSocket->resetCallbacks();
                  /*
                  if (this->mSocket) {
                      SLogger::debug("Resetting socket in onClosed");
                      this->mSocket.reset();
                  } else {
                      SLogger::debug("Socket is nullptr in onClosed");
                  }
                  */
                  this->mSocket->close();

                  SLogger::debug(
                      "Emitting Disconnected in onClosed " +
                      getConnectionTypeString());
                  // const auto gracefulLeave = false;
                  this->emit<Disconnected>(false, 0, "");
                  SLogger::debug(
                      "Removing all listeners in onClosed" +
                      getConnectionTypeString());
                  this->removeAllListeners();
              }
          }},
          onOpen{[this]() {
              std::scoped_lock lock(this->mMutex);
              auto self = this->sharedFromThis<WebsocketConnection>();
              SLogger::trace(
                  "onOpen()",
                  {{"connectionType", getConnectionTypeString()},
                   {"mDestroyed", mDestroyed.load()}});
              if (!self->mDestroyed) {
                  if (self->mSocket &&
                      self->mSocket->readyState() ==
                          rtc::WebSocket::State::Open &&
                      !self->mConnectedEmitted.exchange(true)) {
                      SLogger::trace(
                          "onOpen() emitting Connected " +
                              getConnectionTypeString(),
                          {{"numberofListeners", listenerCount<Connected>()}});

                      self->emit<Connected>();
                  }
              }
          }} {}

    void setSocket(std::shared_ptr<rtc::WebSocket> socket) {
        SLogger::trace("setSocket() " + getConnectionTypeString());
        SLogger::debug("Trying to acquire mutex lock in setSocket");
        std::scoped_lock lock(mMutex);
        SLogger::debug("Acquired mutex lock in setSocket");
        mSocket = std::move(socket);

        SLogger::trace("setSocket() after move " + getConnectionTypeString());

        // Set socket callbacks. The message callback is deliberately NOT
        // attached here — see startReceiving().

        mSocket->onError(this->onError);
        mSocket->onClosed(this->onClosed);
        mSocket->onOpen(this->onOpen);

        // rtc does not retro-fire the open callback: if the websocket
        // handshake completed on the processor thread before the callback
        // above was attached, onOpen would never run and Connected would
        // never be emitted. Emit it here in that case (mConnectedEmitted
        // keeps the two paths idempotent).
        if (mSocket->readyState() == rtc::WebSocket::State::Open) {
            SLogger::trace(
                "setSocket() socket already open, emitting Connected " +
                getConnectionTypeString());
            this->onOpen();
        }

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

    // Attach the rtc message callback and deliver anything received so
    // far. Deliberately separate from setSocket(): incoming frames queue
    // inside libdatachannel until a message callback is attached (they
    // flush synchronously at attach), while our Data event is
    // fire-and-forget — a frame emitted before the application has
    // registered its Data listener is silently lost. The owner therefore
    // calls this only AFTER the application has had the opportunity to
    // register listeners: the server after emitting Connected to the
    // application, the client before open() (its listeners are
    // registered before connect()).
    void startReceiving() {
        SLogger::trace("startReceiving() " + getConnectionTypeString());
        std::scoped_lock lock(mMutex);
        if (!mDestroyed && mSocket) {
            mSocket->onMessage(this->onMessage, this->onStringMessage);
        }
    }

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
        std::shared_ptr<rtc::WebSocket> socket;
        {
            std::scoped_lock lock(mMutex);
            if (mDestroyed) {
                SLogger::debug("close() on destroyed connection");
                return;
            }
            mDestroyed = true;
            socket = mSocket;
        }
        // The rtc calls happen OUTSIDE mMutex: rtc holds its callback
        // mutex while a callback is executing, so resetCallbacks() blocks
        // until any in-flight callback returns — and our rtc callbacks
        // lock mMutex. Calling into rtc while holding mMutex is therefore
        // a lock-order inversion (main thread: mMutex -> callback mutex;
        // rtc thread: callback mutex -> mMutex) that deadlocked test
        // teardowns.
        if (socket) {
            SLogger::debug("close() resetting callbacks");
            socket->resetCallbacks();
            socket->close();
        }
        SLogger::trace(
            "close() end",
            {{"connectionType", getConnectionTypeString()},
             {"mDestroyed", mDestroyed.load()}});
    }

    void destroy() override {
        SLogger::trace(
            "destroy()",
            {{"connectionType", getConnectionTypeString()},
             {"mDestroyed", mDestroyed.load()}});
        std::shared_ptr<rtc::WebSocket> socket;
        {
            std::scoped_lock lock(mMutex);
            if (mDestroyed) {
                SLogger::debug("destroy() on destroyed connection");
                return;
            }
            mDestroyed = true;
            socket = mSocket;
        }
        // Outside mMutex for the same reason as in close().
        if (socket) {
            socket->resetCallbacks();
            socket->close();
        }
    }
};

} // namespace streamr::dht::connection::websocket
