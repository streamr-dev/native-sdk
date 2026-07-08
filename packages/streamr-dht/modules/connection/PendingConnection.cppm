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

export module streamr.dht.PendingConnection;

import streamr.dht.protos;

import streamr.utils.AbortController;
import streamr.utils.AbortableTimers;
import streamr.utils.EnableSharedFromThis;
import streamr.logger.SLogger;
import streamr.dht.Connection;
import streamr.dht.IPendingConnection;
import streamr.dht.Identifiers;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::utils::AbortableTimers;
using streamr::utils::AbortController;
using streamr::utils::EnableSharedFromThis;
// Self-sufficient shorthand (was inherited textually from a
// neighboring header before consolidation).
using streamr::logger::SLogger;

export namespace streamr::dht::connection {

using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::connection::Connection;

class PendingConnection : public IPendingConnection,
                          public EnableSharedFromThis {
private:
    AbortController connectingAbortController;
    PeerDescriptor remotePeerDescriptor;
    std::optional<std::function<void(std::exception_ptr)>> errorCallback;
    std::atomic<bool> errorCallbackCalled = false;
    bool replacedAsDuplicate = false;
    bool stopped = false;

protected:
    explicit PendingConnection(
        PeerDescriptor remotePeerDescriptor,
        std::optional<std::function<void(std::exception_ptr)>> errorCallback =
            std::nullopt)
        : errorCallback(std::move(errorCallback)),
          remotePeerDescriptor(std::move(remotePeerDescriptor)) {}

    // The connect-timeout timer must capture a WEAK self, not `this`: the
    // timer can outlive the PendingConnection (a connect that never completes,
    // e.g. to an offline peer, is torn down while its timeout is still
    // pending), and firing on a freed object would emit Disconnected on a
    // destroyed EventEmitter. Scheduled from newInstance (after make_shared, so
    // sharedFromThis works); the destructor aborts the timer as a backstop.
    void scheduleConnectingTimeout(std::chrono::milliseconds timeout) {
        std::weak_ptr<PendingConnection> weakSelf =
            this->sharedFromThis<PendingConnection>();
        AbortableTimers::setAbortableTimeout(
            [weakSelf]() {
                if (auto self = weakSelf.lock()) {
                    self->close(false);
                }
            },
            timeout,
            this->connectingAbortController.getSignal());
    }

public:
    [[nodiscard]] static std::shared_ptr<PendingConnection> newInstance(
        PeerDescriptor remotePeerDescriptor,
        std::optional<std::function<void(std::exception_ptr)>> errorCallback =
            std::nullopt,
        std::chrono::milliseconds timeout =
            std::chrono::milliseconds(15 * 1000)) { // NOLINT
        struct MakeSharedEnabler : public PendingConnection {
            MakeSharedEnabler(
                PeerDescriptor descriptor,
                std::optional<std::function<void(std::exception_ptr)>> callback)
                : PendingConnection(
                      std::move(descriptor), std::move(callback)) {}
        };
        auto instance = std::make_shared<MakeSharedEnabler>(
            std::move(remotePeerDescriptor), std::move(errorCallback));
        instance->scheduleConnectingTimeout(timeout);
        return instance;
    }

    ~PendingConnection() override { this->connectingAbortController.abort(); }

    void replaceAsDuplicate() override {
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
