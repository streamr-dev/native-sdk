// Module streamr.dht.IncomingHandshaker
// CONSOLIDATED from the former header
// streamr-dht/connection/IncomingHandshaker.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;



export module streamr.dht.IncomingHandshaker;

import std;

import streamr.logger.SLogger;
import streamr.eventemitter.EventEmitter;
import streamr.dht.Connection;
import streamr.dht.Handshaker;
import streamr.dht.IPendingConnection;
import streamr.dht.Identifiers;
import streamr.dht.PendingConnection;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;

export namespace streamr::dht::connection {

using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::PendingConnection;

class IncomingHandshaker : public Handshaker {
private:
    struct Private {
        explicit Private() = default;
    };

    std::optional<std::function<bool(std::shared_ptr<IPendingConnection>)>>
        onNewConnectionCallback;
    std::function<std::shared_ptr<IPendingConnection>(DhtAddress)>
        getPendingConnectionCallback;
    std::shared_ptr<IPendingConnection> pendingConnection;

    HandlerToken disconnectedHandlerToken;
    HandlerToken pendingDisconnectedHandlerToken;

    void stopHandshaker() {
        if (this->pendingConnection) {
            this->pendingConnection->off<pendingconnectionevents::Disconnected>(
                this->pendingDisconnectedHandlerToken);
        }
        this->connection->off<connectionevents::Disconnected>(
            this->disconnectedHandlerToken);
        this->stop();
    }

    void setupPendingConnectionDisconnectedHandler() {
        // weak capture: see Handshaker::registerBaseEventHandlers()
        std::weak_ptr<IncomingHandshaker> weakSelf =
            this->sharedFromThis<IncomingHandshaker>();
        this->pendingDisconnectedHandlerToken =
            this->pendingConnection
                ->once<pendingconnectionevents::Disconnected>(
                    [weakSelf](bool /* gracefulLeave */) {
                        if (auto self = weakSelf.lock()) {
                            self->connection->close(false);
                            self->stopHandshaker();
                        }
                    });
    }

public:
    IncomingHandshaker(
        Private /* prevent direct construction */,
        const PeerDescriptor& localPeerDescriptor,
        const std::shared_ptr<Connection>& connection,
        std::function<std::shared_ptr<IPendingConnection>(DhtAddress)>&&
            getPendingConnectionCallback,
        const std::optional<
            std::function<bool(std::shared_ptr<IPendingConnection>)>>&
            onNewConnectionCallback = std::nullopt)
        : Handshaker(localPeerDescriptor, connection),
          getPendingConnectionCallback(std::move(getPendingConnectionCallback)),
          onNewConnectionCallback(onNewConnectionCallback) {}

    // Called by newInstance() right after construction — see the note
    // on Handshaker::registerBaseEventHandlers() for why the handlers
    // hold weak references instead of raw `this`.
    void registerEventHandlers() {
        std::weak_ptr<IncomingHandshaker> weakSelf =
            this->sharedFromThis<IncomingHandshaker>();
        // disconection of the connection will close the pending connection
        this->disconnectedHandlerToken =
            this->connection->once<connectionevents::Disconnected>(
                [weakSelf](
                    bool gracefulLeave,
                    std::uint64_t /*code*/,
                    const std::string& /*reason*/) {
                    if (auto self = weakSelf.lock()) {
                        if (self->pendingConnection) {
                            self->pendingConnection->close(gracefulLeave);
                        }
                        self->stopHandshaker();
                    }
                });

        // disconnecting pending connection will close the connection
        if (this->pendingConnection) {
            this->setupPendingConnectionDisconnectedHandler();
        }
    }

    [[nodiscard]] static std::shared_ptr<IncomingHandshaker> newInstance(
        const PeerDescriptor& localPeerDescriptor,
        const std::shared_ptr<Connection>& connection,
        std::function<std::shared_ptr<IPendingConnection>(DhtAddress)>&&
            getPendingConnectionCallback,
        const std::optional<
            std::function<bool(std::shared_ptr<IPendingConnection>)>>&
            onNewConnectionCallback = std::nullopt) {
        auto instance = std::make_shared<IncomingHandshaker>(
            Private{},
            localPeerDescriptor,
            connection,
            std::move(getPendingConnectionCallback),
            onNewConnectionCallback);
        instance->registerBaseEventHandlers();
        instance->registerEventHandlers();
        return instance;
    }

    [[nodiscard]] std::shared_ptr<IPendingConnection> getPendingConnection()
        const {
        return this->pendingConnection;
    }

protected:
    void onHandshakeRequest(
        const PeerDescriptor& source,
        const std::string& protocolVersion,
        const std::optional<PeerDescriptor>& target) override {
        if (!isAcceptedVersion(protocolVersion)) {
            this->handleFailure(HandshakeError::UNSUPPORTED_PROTOCOL_VERSION);
            return;
        }

        if (target.has_value() &&
            !Identifiers::areEqualPeerDescriptors(
                this->localPeerDescriptor, target.value())) {
            this->handleFailure(HandshakeError::INVALID_TARGET_PEER_DESCRIPTOR);
            return;
        }

        this->pendingConnection = this->getPendingConnectionCallback(
            Identifiers::getNodeIdFromPeerDescriptor(source));

        if (!this->pendingConnection) {
            this->pendingConnection = PendingConnection::newInstance(source);
            if (this->onNewConnectionCallback) {
                if (!this->onNewConnectionCallback.value()(
                        this->pendingConnection)) {
                    this->handleFailure(HandshakeError::DUPLICATE_CONNECTION);
                    return;
                }
            }
        }
        this->setupPendingConnectionDisconnectedHandler();
        this->handleSuccess(source);
    }

    void onHandshakeResponse(
        const HandshakeResponse& /*handshakeResponse*/) override {
        SLogger::error(
            "IncomingHandshaker received a handshake response, this shoud never happen");
        stopHandshaker();
    }

    void handleSuccess(const PeerDescriptor& peerDescriptor) override {
        this->sendHandshakeResponse();

        if (this->pendingConnection) {
            this->pendingConnection->onHandshakeCompleted(this->connection);
        }

        this->emit<handshakerevents::HandshakeCompleted>(peerDescriptor);
        this->stopHandshaker();
    }

    void handleFailure(std::optional<HandshakeError> error) override {
        this->sendHandshakeResponse(error);

        if (this->pendingConnection) {
            this->pendingConnection->destroy();
        }

        this->connection->destroy();
        this->stopHandshaker();
    }
};

} // namespace streamr::dht::connection
