// Module streamr.dht.OutgoingHandshaker
// CONSOLIDATED from the former header
// streamr-dht/connection/OutgoingHandshaker.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;



export module streamr.dht.OutgoingHandshaker;

import std;

import streamr.dht.IPendingConnection;
import streamr.logger.SLogger;
import streamr.eventemitter.EventEmitter;
import streamr.dht.Connection;
import streamr.dht.Handshaker;
import streamr.dht.Identifiers;
import streamr.dht.PendingConnection;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;

export namespace streamr::dht::connection {

using streamr::dht::Identifiers;
using streamr::dht::connection::Connection;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::PendingConnection;

class OutgoingHandshaker : public Handshaker {
private:
    struct Private {
        explicit Private() = default;
    };
    PeerDescriptor targetPeerDescriptor;
    std::shared_ptr<IPendingConnection> pendingConnection;

    HandlerToken connectedHandlerToken;
    HandlerToken disconnectedHandlerToken;
    HandlerToken pendingDisconnectedHandlerToken;
    HandlerToken errorHandlerToken;

    void stopHandshaker() {
        this->connection->off<connectionevents::Connected>(
            this->connectedHandlerToken);
        this->connection->off<connectionevents::Disconnected>(
            this->disconnectedHandlerToken);
        this->connection->off<connectionevents::Error>(this->errorHandlerToken);
        this->pendingConnection->off<pendingconnectionevents::Disconnected>(
            this->pendingDisconnectedHandlerToken);

        this->stop();
    }

public:
    OutgoingHandshaker(
        Private /* prevent direct construction */,
        const PeerDescriptor& localPeerDescriptor,
        const std::shared_ptr<Connection>& connection,
        PeerDescriptor targetPeerDescriptor,
        const std::shared_ptr<IPendingConnection>& pendingConnection)
        : Handshaker(localPeerDescriptor, connection),
          targetPeerDescriptor(std::move(targetPeerDescriptor)),
          pendingConnection(pendingConnection) {
        SLogger::debug(
            "OutgoingHandshaker() created for " +
            this->targetPeerDescriptor.DebugString());
    }

    // Called by newInstance() right after construction — see the note
    // on Handshaker::registerBaseEventHandlers() for why the handlers
    // hold weak references instead of raw `this`.
    void registerEventHandlers() {
        std::weak_ptr<OutgoingHandshaker> weakSelf =
            this->sharedFromThis<OutgoingHandshaker>();
        // send handshake request when the connection is established
        this->connectedHandlerToken =
            this->connection->once<connectionevents::Connected>([weakSelf]() {
                if (auto self = weakSelf.lock()) {
                    self->sendHandshakeRequest(self->targetPeerDescriptor);
                }
            });

        // disconection of the connection will close the pending connection
        this->disconnectedHandlerToken =
            this->connection->once<connectionevents::Disconnected>(
                [weakSelf](
                    bool gracefulLeave,
                    std::uint64_t /*code*/,
                    const std::string& /*reason*/) {
                    if (auto self = weakSelf.lock()) {
                        self->pendingConnection->close(gracefulLeave);
                        self->stopHandshaker();
                    }
                });

        this->errorHandlerToken =
            this->connection->once<connectionevents::Error>(
                [weakSelf](const std::string& name) {
                    SLogger::error("OutgoingHandshaker got error: " + name);
                    if (auto self = weakSelf.lock()) {
                        self->pendingConnection->onError(
                            std::make_exception_ptr(std::runtime_error(name)));
                    }
                });
        // disconnecting pending connection will close the connection
        this->pendingDisconnectedHandlerToken =
            this->pendingConnection
                ->once<pendingconnectionevents::Disconnected>(
                    [weakSelf](bool /*gracefulLeave*/) {
                        if (auto self = weakSelf.lock()) {
                            self->connection->close(false);
                            self->stopHandshaker();
                        }
                    });
    }

    [[nodiscard]] static std::shared_ptr<OutgoingHandshaker> newInstance(
        const PeerDescriptor& localPeerDescriptor,
        const std::shared_ptr<Connection>& connection,
        const PeerDescriptor& targetPeerDescriptor,
        const std::shared_ptr<IPendingConnection>& pendingConnection) {
        auto instance = std::make_shared<OutgoingHandshaker>(
            Private{},
            localPeerDescriptor,
            connection,
            targetPeerDescriptor,
            pendingConnection);
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
        const PeerDescriptor& /*source*/,
        const std::string& /*protocolVersion*/,
        const std::optional<PeerDescriptor>& /*target*/) override {
        SLogger::error(
            "OutgoingHandshaker received a handshake request, this shoud never happen");
        stopHandshaker();
    }

    void onHandshakeResponse(
        const HandshakeResponse& handshakeResponse) override {
        // An errored response is routed through handleFailure(), NOT
        // closed directly: handleFailure() closes the pending connection
        // only for INVALID_TARGET_PEER_DESCRIPTOR /
        // UNSUPPORTED_PROTOCOL_VERSION and otherwise waits for the other
        // end to close the connection (matching the TS handshaker). This
        // matters for DUPLICATE_CONNECTION — the peer won the
        // simultaneous-connect tie-break and rejected this outgoing
        // connection — where closing here would tear this endpoint down
        // (and drop its buffered messages) even though it is about to be
        // served by the peer's incoming connection. Directly closing on
        // any error was the cause of the ~1-in-30 `received2` message loss
        // in the SimultaneousConnections stress runs (phase A0).
        std::optional<HandshakeError> error;
        if (!isAcceptedVersion(handshakeResponse.protocolversion())) {
            error = HandshakeError::UNSUPPORTED_PROTOCOL_VERSION;
        } else if (handshakeResponse.has_error()) {
            error = handshakeResponse.error();
        }
        if (error.has_value()) {
            this->handleFailure(error);
        } else {
            this->handleSuccess(handshakeResponse.sourcepeerdescriptor());
        }
    }

    void handleSuccess(const PeerDescriptor& peerDescriptor) override {
        this->emit<handshakerevents::HandshakeCompleted>(peerDescriptor);
        SLogger::trace(
            "handshake completed for outgoing connection, " +
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor));
        this->pendingConnection->onHandshakeCompleted(this->connection);
        stopHandshaker();
    }

    void handleFailure(std::optional<HandshakeError> error) override {
        this->emit<handshakerevents::HandshakeFailed>(error);
        if (error.has_value() &&
            (error.value() == HandshakeError::INVALID_TARGET_PEER_DESCRIPTOR ||
             error.value() == HandshakeError::UNSUPPORTED_PROTOCOL_VERSION)) {
            SLogger::trace(
                "handshake failed for outgoing connection, " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->targetPeerDescriptor) +
                " closing pending connection");
            this->pendingConnection->close(false);
            stopHandshaker();
        } else {
            SLogger::trace(
                "handshake failed for outgoing connection, " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->targetPeerDescriptor) +
                " waiting for the other end to close the connection");
            // The other end won the simultaneous-connect tie-break
            // (DUPLICATE_CONNECTION). Silence this pending connection now,
            // on receipt of the error, so that when its connection is
            // subsequently closed by the peer the close does NOT tear down
            // our endpoint: the endpoint is about to be handed the peer's
            // winning connection (with its buffered messages intact). The
            // error response is delivered before that close on the same
            // association (per-association FIFO), so the silencing is
            // always in place in time — this is what makes the convergence
            // race-free rather than merely likely to win the race.
            this->pendingConnection->replaceAsDuplicate();
        }
    }
};

} // namespace streamr::dht::connection
