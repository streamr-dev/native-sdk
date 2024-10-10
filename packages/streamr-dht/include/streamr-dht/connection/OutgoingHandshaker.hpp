#ifndef STREAMR_DHT_CONNECTION_OUTGOING_HANDSHAKER_HPP
#define STREAMR_DHT_CONNECTION_OUTGOING_HANDSHAKER_HPP

#include <memory>
#include <optional>
#include <string>

#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/Handshaker.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"

namespace streamr::dht::connection {

using streamr::dht::Identifiers;
using streamr::dht::connection::Connection;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::IPendingConnection;
using streamr::eventemitter::HandlerToken;

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
        // send handshake request when the connection is established
        this->connectedHandlerToken =
            this->connection->once<connectionevents::Connected>([this]() {
                this->sendHandshakeRequest(this->targetPeerDescriptor);
            });

        // disconection of the connection will close the pending connection
        this->disconnectedHandlerToken =
            this->connection->once<connectionevents::Disconnected>(
                [this](
                    bool gracefulLeave,
                    uint64_t /*code*/,
                    const std::string& /*reason*/) {
                    auto self = this->sharedFromThis<OutgoingHandshaker>();
                    this->pendingConnection->close(gracefulLeave);
                    stopHandshaker();
                });

        this->errorHandlerToken =
            this->connection->once<connectionevents::Error>(
                [this](const std::string& name) {
                    SLogger::error("OutgoingHandshaker got error: " + name);
                    auto self = this->sharedFromThis<OutgoingHandshaker>();
                    this->pendingConnection->onError(
                        std::make_exception_ptr(std::runtime_error(name)));
                });
        // disconnecting pending connection will close the connection
        this->pendingDisconnectedHandlerToken =
            this->pendingConnection
                ->once<pendingconnectionevents::Disconnected>(
                    [this](bool /*gracefulLeave*/) {
                        auto self = this->sharedFromThis<OutgoingHandshaker>();
                        this->connection->close(false);
                        stopHandshaker();
                    });
    }

    [[nodiscard]] static std::shared_ptr<OutgoingHandshaker> newInstance(
        const PeerDescriptor& localPeerDescriptor,
        const std::shared_ptr<Connection>& connection,
        const PeerDescriptor& targetPeerDescriptor,
        const std::shared_ptr<IPendingConnection>& pendingConnection) {
        return std::make_shared<OutgoingHandshaker>(
            Private{},
            localPeerDescriptor,
            connection,
            targetPeerDescriptor,
            pendingConnection);
    }

    [[nodiscard]] std::shared_ptr<IPendingConnection> getPendingConnection()
        const {
        return this->pendingConnection;
    }

protected:
    void onHandshakeRequest(
        const PeerDescriptor& /*source*/,
        const std::string& /*version*/,
        const std::optional<PeerDescriptor>& /*target*/) override {
        SLogger::error(
            "OutgoingHandshaker received a handshake request, this shoud never happen");
        stopHandshaker();
    }

    void onHandshakeResponse(
        const HandshakeResponse& handshakeResponse) override {
        if (handshakeResponse.has_error()) {
            this->pendingConnection->close(false);
            stopHandshaker();
        } else if (!isAcceptedVersion(handshakeResponse.version())) {
            this->handleFailure(HandshakeError::UNSUPPORTED_VERSION);
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
             error.value() == HandshakeError::UNSUPPORTED_VERSION)) {
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
        }
    }
};

} // namespace streamr::dht::connection

#endif