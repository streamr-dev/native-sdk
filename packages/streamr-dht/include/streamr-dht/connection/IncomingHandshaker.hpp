#ifndef STREAMR_DHT_CONNECTION_INCOMING_HANDSHAKER_HPP
#define STREAMR_DHT_CONNECTION_INCOMING_HANDSHAKER_HPP

#include <memory>
#include <optional>
#include <string>

#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/Handshaker.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"

namespace streamr::dht::connection {

using streamr::dht::connection::PendingConnection;
using streamr::eventemitter::HandlerToken;

class IncomingHandshaker : public Handshaker {
private:
    struct Private {
        explicit Private() = default;
    };

    std::optional<std::function<bool(std::shared_ptr<PendingConnection>)>>
        onNewConnectionCallback;
    std::function<std::shared_ptr<PendingConnection>(DhtAddress)>
        getPendingConnectionCallback;
    std::shared_ptr<PendingConnection> pendingConnection;

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
        this->pendingDisconnectedHandlerToken =
            this->pendingConnection
                ->once<pendingconnectionevents::Disconnected>(
                    [this](bool /* gracefulLeave */) {
                        this->connection->close(false);
                        this->stopHandshaker();
                    });
    }

public:
    IncomingHandshaker(
        Private /* prevent direct construction */,
        const PeerDescriptor& localPeerDescriptor,
        const std::shared_ptr<Connection>& connection,
        std::function<std::shared_ptr<PendingConnection>(DhtAddress)>&&
            getPendingConnectionCallback,
        const std::optional<
            std::function<bool(std::shared_ptr<PendingConnection>)>>&
            onNewConnectionCallback = std::nullopt)
        : Handshaker(localPeerDescriptor, connection),
          getPendingConnectionCallback(std::move(getPendingConnectionCallback)),
          onNewConnectionCallback(onNewConnectionCallback) {
        // disconection of the connection will close the pending connection
        this->disconnectedHandlerToken =
            this->connection->once<connectionevents::Disconnected>(
                [this](
                    bool gracefulLeave,
                    uint64_t /*code*/,
                    const std::string& /*reason*/) {
                    if (this->pendingConnection) {
                        this->pendingConnection->close(gracefulLeave);
                    }
                    this->stopHandshaker();
                });

        // disconnecting pending connection will close the connection
        if (this->pendingConnection) {
            this->setupPendingConnectionDisconnectedHandler();
        }
    }

    [[nodiscard]] static std::shared_ptr<IncomingHandshaker> newInstance(
        const PeerDescriptor& localPeerDescriptor,
        const std::shared_ptr<Connection>& connection,
        std::function<std::shared_ptr<PendingConnection>(DhtAddress)>&&
            getPendingConnectionCallback,
        const std::optional<
            std::function<bool(std::shared_ptr<PendingConnection>)>>&
            onNewConnectionCallback = std::nullopt) {
        return std::make_shared<IncomingHandshaker>(
            Private{},
            localPeerDescriptor,
            connection,
            std::move(getPendingConnectionCallback),
            onNewConnectionCallback);
    }

    [[nodiscard]] std::shared_ptr<PendingConnection> getPendingConnection()
        const {
        return this->pendingConnection;
    }

protected:
    void onHandshakeRequest(
        const PeerDescriptor& source,
        const std::string& version,
        const std::optional<PeerDescriptor>& target) override {
        if (!isAcceptedVersion(version)) {
            this->handleFailure(HandshakeError::UNSUPPORTED_VERSION);
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
            this->pendingConnection =
                std::make_shared<PendingConnection>(source);
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

#endif
