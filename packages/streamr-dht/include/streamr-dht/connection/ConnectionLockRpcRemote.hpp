#ifndef STREAMR_DHT_CONNECTION_CONNECTIONLOCKRPCREMOTE_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONLOCKRPCREMOTE_HPP

#include <chrono>
#include <optional>
#include <folly/coro/Task.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/dht/protos/DhtRpc.client.pb.h"
#include "streamr-dht/dht/contact/RpcRemote.hpp"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-dht/connection/ConnectionLockStates.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"

namespace streamr::dht::connection {

using ::dht::ConnectionLockRpcClient;
using ::dht::PeerDescriptor;
using ::dht::LockRequest;
using ::dht::UnlockRequest;
using ::dht::DisconnectNotice;
using ::dht::DisconnectMode;
using streamr::dht::contact::RpcRemote;
using streamr::dht::connection::LockID;
using streamr::dht::rpcprotocol::DhtCallContext;

class ConnectionLockRpcRemote : public RpcRemote<ConnectionLockRpcClient> {

public:
ConnectionLockRpcRemote(
        const PeerDescriptor& localPeerDescriptor,  // NOLINT
        const PeerDescriptor& remotePeerDescriptor,
        ConnectionLockRpcClient& client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<ConnectionLockRpcClient>(localPeerDescriptor, remotePeerDescriptor, client, timeout) {
    }


    folly::coro::Task<bool> lockRequest(const LockID& lockId) {
        SLogger::trace("Requesting locked connection to " + Identifiers::getNodeIdFromPeerDescriptor(this->getPeerDescriptor()));
        LockRequest request;
        request.set_lockid(lockId);


        const auto options = this->formDhtRpcOptions();
        try {
            const auto res = co_await this->getClient().lockRequest(request, options);
            co_return res.accepted();
        } catch (const std::exception& err) {
            SLogger::debug("Connection lock rejected " + std::string(err.what()));
            co_return false;
        }
    }

    folly::coro::Task<void> unlockRequest(const LockID& lockId) {
        SLogger::trace("Requesting connection to be unlocked from " + Identifiers::getNodeIdFromPeerDescriptor(this->getPeerDescriptor()));
        UnlockRequest request;
        request.set_lockid(lockId);
        const auto options = this->formDhtRpcOptions();
        try {
            co_await this->getClient().unlockRequest(request, options);
        } catch (const std::exception& err) {
            SLogger::trace("failed to send unlockRequest " + std::string(err.what()));
        }
    }

    folly::coro::Task<void> gracefulDisconnect(const DisconnectMode& disconnectMode) {
        SLogger::trace("Notifying a graceful disconnect to " + Identifiers::getNodeIdFromPeerDescriptor(this->getPeerDescriptor()));
        DisconnectNotice request;
        request.set_disconnectmode(disconnectMode);
        
        DhtCallContext context;
        context.connect = false;
        context.sendIfStopped = true;
        context.timeout = std::chrono::milliseconds(2000); // NOLINT

        const auto options = this->formDhtRpcOptions(context);

        co_await this->getClient().gracefulDisconnect(request, options);
    }


};

} // namespace streamr::dht::connection

#endif