#ifndef STREAMR_DHT_CONNECTION_CONNECTIONLOCKRPCLOCAL_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONLOCKRPCLOCAL_HPP

#include <functional>
#include <optional>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/dht/protos/DhtRpc.server.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/ConnectionLockStates.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-logger/SLogger.hpp"
namespace streamr::dht::connection {

using ::dht::ConnectionLockRpc;
using ::dht::DisconnectMode;
using ::dht::DisconnectNotice;
using ::dht::LockRequest;
using ::dht::LockResponse;
using ::dht::PeerDescriptor;
using ::dht::UnlockRequest;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::logger::SLogger;

struct ConnectionLockRpcLocalOptions {
    std::function<void(const DhtAddress&, const LockID&)> addRemoteLocked;
    std::function<void(const DhtAddress&, const LockID&)> removeRemoteLocked;
    std::function<void(
        const PeerDescriptor&, bool, const std::optional<std::string>&)>
        closeConnection;
    std::function<PeerDescriptor()> getLocalPeerDescriptor;
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
        const auto senderPeerDescriptor =
            static_cast<const DhtCallContext&>(callContext)
                .incomingSourceDescriptor;

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
        const auto senderPeerDescriptor =
            static_cast<const DhtCallContext&>(callContext)
                .incomingSourceDescriptor;
        const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
            senderPeerDescriptor.value());
        this->options.removeRemoteLocked(nodeId, LockID{request.lockid()});
    }

    void gracefulDisconnect(
        const DisconnectNotice& request,
        const DhtCallContext& callContext) override {
        const auto senderPeerDescriptor =
            static_cast<const DhtCallContext&>(callContext)
                .incomingSourceDescriptor;
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
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_CONNECTIONLOCKRPCLOCAL_HPP