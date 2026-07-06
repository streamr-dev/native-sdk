// Module streamr.dht.ConnectionLockRpcLocal
// CONSOLIDATED from the former header
// streamr-dht/connection/ConnectionLockRpcLocal.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;

#include <functional>
#include <optional>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <string>

export module streamr.dht.ConnectionLockRpcLocal;

import streamr.dht.DhtRpcServer;
import streamr.logger.SLogger;
import streamr.dht.ConnectionLockStates;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
export namespace streamr::dht::connection {

using ::dht::ConnectionLockRpc;
using ::dht::DisconnectMode;
using ::dht::DisconnectNotice;
using ::dht::LockRequest;
using ::dht::LockResponse;
using ::dht::PeerDescriptor;
using ::dht::SetPrivateRequest;
using ::dht::UnlockRequest;
using streamr::dht::rpcprotocol::DhtCallContext;

struct ConnectionLockRpcLocalOptions {
    std::function<void(const DhtAddress&, const LockID&)> addRemoteLocked;
    std::function<void(const DhtAddress&, const LockID&)> removeRemoteLocked;
    std::function<void(
        const PeerDescriptor&, bool, const std::optional<std::string>&)>
        closeConnection;
    std::function<PeerDescriptor()> getLocalPeerDescriptor;
    std::function<void(const DhtAddress&, bool)> setPrivate;
};

class ConnectionLockRpcLocal : public ConnectionLockRpc<DhtCallContext> {
private:
    ConnectionLockRpcLocalOptions options;

public:
    explicit ConnectionLockRpcLocal(ConnectionLockRpcLocalOptions&& options)
        : options(std::move(options)) {}

    ~ConnectionLockRpcLocal() override = default;

    LockResponse lockRequest(
        const LockRequest& request,
        const DhtCallContext& callContext) override {
        const auto senderPeerDescriptor = callContext.incomingSourceDescriptor;

        if (Identifiers::areEqualPeerDescriptors(
                senderPeerDescriptor.value(),
                this->options.getLocalPeerDescriptor())) {
            LockResponse response;
            response.set_accepted(false);
            return response;
        }

        const auto remoteNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            senderPeerDescriptor.value());
        this->options.addRemoteLocked(remoteNodeId, LockID{request.lockid()});
        LockResponse response;
        response.set_accepted(true);

        return response;
    }

    void unlockRequest(
        const UnlockRequest& request,
        const DhtCallContext& callContext) override {
        const auto senderPeerDescriptor = callContext.incomingSourceDescriptor;
        const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
            senderPeerDescriptor.value());
        this->options.removeRemoteLocked(nodeId, LockID{request.lockid()});
    }

    void gracefulDisconnect(
        const DisconnectNotice& request,
        const DhtCallContext& callContext) override {
        const auto senderPeerDescriptor = callContext.incomingSourceDescriptor;
        SLogger::trace(
            Identifiers::getNodeIdFromPeerDescriptor(
                senderPeerDescriptor.value()) +
            " received gracefulDisconnect notice");

        if (request.disconnectmode() == DisconnectMode::LEAVING) {
            this->options.closeConnection(
                senderPeerDescriptor.value(), true, "graceful leave notified");
        } else {
            this->options.closeConnection(
                senderPeerDescriptor.value(),
                false,
                "graceful disconnect notified");
        }
    }

    void setPrivate(
        const SetPrivateRequest& request,
        const DhtCallContext& callContext) override {
        const auto senderPeerDescriptor = callContext.incomingSourceDescriptor;
        const auto senderId = Identifiers::getNodeIdFromPeerDescriptor(
            senderPeerDescriptor.value());
        this->options.setPrivate(senderId, request.isprivate());
    }
};

} // namespace streamr::dht::connection
