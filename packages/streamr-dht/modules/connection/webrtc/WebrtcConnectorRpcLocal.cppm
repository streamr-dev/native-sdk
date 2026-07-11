// Module streamr.dht.WebrtcConnectorRpcLocal
// Ported from packages/dht/src/connection/webrtc/WebrtcConnectorRpcLocal.ts
// (v103.8.0-rc.3): the receiving side of the WebRTC signalling
// notifications. Adaptation: the TS options pass the connector's mutable
// ongoingConnectAttempts map; here the connector provides a thread-safe
// accessor callback instead (its own mutex guards the map — the TS
// single-threaded event loop needed none).
module;

#include <functional>
#include <memory>
#include <optional>
#include <string>

export module streamr.dht.WebrtcConnectorRpcLocal;

import streamr.dht.protos;

import streamr.dht.AddressTools;
import streamr.dht.Connection;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtRpcServer;
import streamr.dht.Identifiers;
import streamr.dht.IPendingConnection;
import streamr.dht.WebrtcConnection;
import streamr.dht.webrtcTypes;
import streamr.logger.SLogger;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht::connection::webrtc {

using ::dht::IceCandidate;
using ::dht::PeerDescriptor;
using ::dht::RtcAnswer;
using ::dht::RtcOffer;
using ::dht::WebrtcConnectionRequest;
using ::dht::WebrtcConnectorRpc;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionID;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::helpers::AddressTools;
using streamr::dht::rpcprotocol::DhtCallContext;

// TS ConnectingConnection (WebrtcConnector.ts).
struct ConnectingConnection {
    std::shared_ptr<IPendingConnection> managedConnection;
    std::shared_ptr<WebrtcConnection> connection;
};

struct WebrtcConnectorRpcLocalOptions {
    std::function<std::shared_ptr<IPendingConnection>(
        const PeerDescriptor& /*targetPeerDescriptor*/,
        bool /*doNotRequestConnection*/)>
        connect;
    std::function<bool(const std::shared_ptr<IPendingConnection>&)>
        onNewConnection;
    // Thread-safe accessor into the connector's ongoingConnectAttempts.
    std::function<std::optional<ConnectingConnection>(const DhtAddress&)>
        getOngoingConnectAttempt;
    std::function<PeerDescriptor()> getLocalPeerDescriptor;
    bool allowPrivateAddresses;
};

class WebrtcConnectorRpcLocal : public WebrtcConnectorRpc<DhtCallContext> {
private:
    WebrtcConnectorRpcLocalOptions options;

public:
    explicit WebrtcConnectorRpcLocal(WebrtcConnectorRpcLocalOptions&& options)
        : options(std::move(options)) {}
    ~WebrtcConnectorRpcLocal() override = default;

    void requestConnection(
        const WebrtcConnectionRequest& /*request*/,
        const DhtCallContext& callContext) override {
        const auto targetPeerDescriptor =
            callContext.incomingSourceDescriptor.value();
        if (this->options
                .getOngoingConnectAttempt(
                    Identifiers::getNodeIdFromPeerDescriptor(
                        targetPeerDescriptor))
                .has_value()) {
            return;
        }
        const auto pendingConnection =
            this->options.connect(targetPeerDescriptor, false);
        this->options.onNewConnection(pendingConnection);
    }

    void rtcOffer(
        const RtcOffer& request, const DhtCallContext& callContext) override {
        const auto remotePeerDescriptor =
            callContext.incomingSourceDescriptor.value();
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(remotePeerDescriptor);

        auto existing = this->options.getOngoingConnectAttempt(nodeId);
        std::shared_ptr<WebrtcConnection> connection;
        if (!existing.has_value()) {
            const auto pendingConnection =
                this->options.connect(remotePeerDescriptor, true);
            connection =
                this->options.getOngoingConnectAttempt(nodeId)->connection;
            this->options.onNewConnection(pendingConnection);
        } else {
            connection = existing->connection;
        }
        // Always use the offerer's connectionId.
        connection->setConnectionId(ConnectionID{request.connectionid()});
        connection->setRemoteDescription(
            request.description(), rtcdescription::OFFER);
    }

    void rtcAnswer(
        const RtcAnswer& request, const DhtCallContext& callContext) override {
        const auto remotePeerDescriptor =
            callContext.incomingSourceDescriptor.value();
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(remotePeerDescriptor);
        const auto existing = this->options.getOngoingConnectAttempt(nodeId);
        if (!existing.has_value()) {
            return;
        }
        if (existing->connection->getConnectionID() != request.connectionid()) {
            SLogger::trace("Ignoring RTC answer due to connectionId mismatch");
            return;
        }
        existing->connection->setRemoteDescription(
            request.description(), rtcdescription::ANSWER);
    }

    void iceCandidate(
        const IceCandidate& request,
        const DhtCallContext& callContext) override {
        const auto remotePeerDescriptor =
            callContext.incomingSourceDescriptor.value();
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(remotePeerDescriptor);
        const auto existing = this->options.getOngoingConnectAttempt(nodeId);
        if (!existing.has_value()) {
            return;
        }
        if (existing->connection->getConnectionID() != request.connectionid()) {
            SLogger::trace(
                "Ignoring remote candidate due to connectionId mismatch");
            return;
        }
        if (this->isIceCandidateAllowed(request.candidate())) {
            existing->connection->addRemoteCandidate(
                request.candidate(), request.mid());
        }
    }

private:
    [[nodiscard]] bool isIceCandidateAllowed(
        const std::string& candidate) const {
        if (!this->options.allowPrivateAddresses) {
            const auto address =
                AddressTools::getAddressFromIceCandidate(candidate);
            if (address.has_value() && AddressTools::isPrivateIPv4(*address)) {
                return false;
            }
        }
        return true;
    }
};

} // namespace streamr::dht::connection::webrtc
