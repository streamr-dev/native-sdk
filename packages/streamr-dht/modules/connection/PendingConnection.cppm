// Module streamr.dht.PendingConnection
// CONSOLIDATED from the former header
// streamr-dht/connection/PendingConnection.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <atomic>
#include <chrono>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.PendingConnection;

import streamr.utils.AbortController;
import streamr.utils.AbortableTimers;
import streamr.logger;
import streamr.dht.Connection;
import streamr.dht.IPendingConnection;
import streamr.dht.Identifiers;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::utils::AbortableTimers;
using streamr::utils::AbortController;
// Self-sufficient shorthand (was inherited textually from a
// neighboring header before consolidation).
using streamr::logger::SLogger;

export namespace streamr::dht::connection {

using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::connection::Connection;

class PendingConnection : public IPendingConnection {
private:
    AbortController connectingAbortController;
    PeerDescriptor remotePeerDescriptor;
    std::optional<std::function<void(std::exception_ptr)>> errorCallback;
    std::atomic<bool> errorCallbackCalled = false;
    bool replacedAsDuplicate = false;
    bool stopped = false;

public:
    explicit PendingConnection(
        PeerDescriptor remotePeerDescriptor,
        std::optional<std::function<void(std::exception_ptr)>> errorCallback =
            std::nullopt,
        std::chrono::milliseconds timeout =
            std::chrono::milliseconds(15 * 1000)) // NOLINT
        : errorCallback(std::move(errorCallback)),
          remotePeerDescriptor(std::move(remotePeerDescriptor)) {
        AbortableTimers::setAbortableTimeout(
            [this]() { this->close(false); },
            timeout,
            this->connectingAbortController.getSignal());
    }

    void replaceAsDuplicate() {
        SLogger::trace(
            Identifiers::getNodeIdFromPeerDescriptor(
                this->remotePeerDescriptor) +
            " replaceAsDuplicate");
        this->replacedAsDuplicate = true;
    }

    void onHandshakeCompleted(
        const std::shared_ptr<Connection>& connection) override {
        this->connectingAbortController.abort();
        if (!this->replacedAsDuplicate) {
            this->emit<pendingconnectionevents::Connected>(
                this->remotePeerDescriptor, connection);
        }
    }

    void onError(const std::exception_ptr& error) override {
        SLogger::error(
            Identifiers::getNodeIdFromPeerDescriptor(
                this->remotePeerDescriptor) +
            " PendingConnection onError");
        if (this->errorCallback.has_value() && !this->errorCallbackCalled) {
            SLogger::error(
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->remotePeerDescriptor) +
                " PendingConnection onError calling errorCallback");
            this->errorCallback.value()(error);
            this->errorCallbackCalled = true;
        }
    }

    void close(bool graceful) override {
        if (this->stopped) {
            return;
        }
        if (this->errorCallback.has_value() && !this->errorCallbackCalled) {
            SLogger::error(
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->remotePeerDescriptor) +
                " PendingConnection onError calling errorCallback");
            this->errorCallback.value()(std::make_exception_ptr(
                std::runtime_error(
                    "PendingConnection closed while connecting")));
            this->errorCallbackCalled = true;
        }

        this->stopped = true;
        this->connectingAbortController.abort();
        if (!this->replacedAsDuplicate) {
            this->emit<pendingconnectionevents::Disconnected>(graceful);
        }
    }

    void destroy() override {
        if (this->stopped) {
            return;
        }
        this->stopped = true;
        this->connectingAbortController.abort();
        this->removeAllListeners();
    }

    [[nodiscard]] const PeerDescriptor& getPeerDescriptor() const override {
        return this->remotePeerDescriptor;
    }
};

} // namespace streamr::dht::connection
