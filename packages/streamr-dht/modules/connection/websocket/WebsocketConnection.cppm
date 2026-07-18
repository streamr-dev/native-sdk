// Module streamr.dht.WebsocketConnection
// CONSOLIDATED from the former header
// streamr-dht/connection/websocket/WebsocketConnection.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;

#include <functional>
#include <rtc/rtc.hpp>

#include <folly/synchronization/Baton.h>

#include <string>

export module streamr.dht.WebsocketConnection;

import streamr.logger.SLogger;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.SharedExecutors;
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

inline constexpr size_t maxMessageSize = 1048576;

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

private:
    // All libdatachannel callbacks are dispatched through this per-connection
    // serial view of the shared worker pool instead of running inline. Every
    // websocket in the process shares ONE libdatachannel poll thread; running
    // the emit chains (handshake, RPC parse/dispatch) inline there starved
    // concurrent websocket upgrades so badly that a 12-node start storm
    // pushed connect latency past the 1 s connectivity-check timeout
    // (measured: the poll thread was >95% busy in Data emit chains while
    // accepts sat waiting). The serial executor preserves the per-connection
    // event order the emitter contract relies on: Connected before any Data,
    // Data in arrival order, Disconnected last (the same per-association
    // ordering idiom as the Simulator's delivery executors).
    streamr::utils::SharedSerialExecutor mDispatchExecutor{
        streamr::utils::SharedExecutors::worker()};

    // Enqueue a callback body; the task keeps the connection alive until it
    // has run. Callers are live rtc callbacks (rtc's callback mutex
    // guarantees the object cannot be mid-destruction there — the
    // destructor's resetCallbacks() blocks on in-flight callbacks) or
    // methods invoked through an owning shared_ptr (setSocket's
    // already-open path), so sharedFromThis is safe.
    void dispatch(std::function<void()> task) {
        auto self = this->sharedFromThis<WebsocketConnection>();
        mDispatchExecutor.add([self, task = std::move(task)]() { task(); });
    }

protected:
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

        // Set socket callbacks. Each is a thin wrapper that only enqueues
        // the real body onto the per-connection dispatch executor — the rtc
        // callback threads (the shared poll thread, the per-server accept
        // thread) must never run the emit chains (see mDispatchExecutor).
        // The message callback is deliberately NOT attached here — see
        // startReceiving().

        mSocket->onError([this](std::string error) {
            this->dispatch(
                [this, error = std::move(error)]() { this->onError(error); });
        });
        mSocket->onClosed(
            [this]() { this->dispatch([this]() { this->onClosed(); }); });
        mSocket->onOpen(
            [this]() { this->dispatch([this]() { this->onOpen(); }); });

        // rtc does not retro-fire the open callback: if the websocket
        // handshake completed on the processor thread before the callback
        // above was attached, onOpen would never run and Connected would
        // never be emitted. Enqueue it here in that case (mConnectedEmitted
        // keeps the two paths idempotent).
        if (mSocket->readyState() == rtc::WebSocket::State::Open) {
            SLogger::trace(
                "setSocket() socket already open, emitting Connected " +
                getConnectionTypeString());
            this->dispatch([this]() { this->onOpen(); });
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
            // Thin wrapper (see setSocket): the poll thread only copies the
            // frame into a task; the emit<Data> chain runs on the dispatch
            // executor. The attach-time flush of queued frames enqueues
            // them here in order, ahead of any later-arriving frame.
            mSocket->onMessage(
                [this](rtc::binary data) {
                    this->dispatch([this, data = std::move(data)]() mutable {
                        this->onMessage(std::move(data));
                    });
                },
                this->onStringMessage);
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

    // Barrier: returns once every dispatch task enqueued so far has run.
    // resetCallbacks() only stops NEW rtc callbacks; already-enqueued
    // bodies still run afterwards, so an owner whose listeners capture it
    // by raw pointer must drain before destroying itself (WebsocketServer
    // does, for half-ready connections). MUST NOT be called from a task
    // running on this connection's own dispatch executor — that would
    // deadlock the serial queue.
    void drainDispatchQueue() {
        folly::Baton<> baton;
        mDispatchExecutor.add([&baton]() { baton.post(); });
        baton.wait();
    }
};

} // namespace streamr::dht::connection::websocket
