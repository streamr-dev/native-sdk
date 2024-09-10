#ifndef STREAMR_DHT_CONNECTION_CONNECTIONLOCKRPCREMOTE_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONLOCKRPCREMOTE_HPP

#include <chrono>
#include <optional>
#include <folly/coro/Task.h>
#include "packages/dht/protos/DhtRpc.client.pb.h"
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/ConnectionLockStates.hpp"
#include "streamr-dht/dht/contact/RpcRemote.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-logger/SLogger.hpp"

namespace streamr::dht::connection {

using streamr::dht::rpcprotocol::DhtCallContext;
using ConnectionLockRpcClient = ::dht::ConnectionLockRpcClient<DhtCallContext>;
using PeerDescriptor = ::dht::PeerDescriptor;
using LockRequest = ::dht::LockRequest;
using ::dht::DisconnectMode;
using ::dht::DisconnectNotice;
using ::dht::UnlockRequest;
using streamr::dht::connection::LockID;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;

class ConnectionLockRpcRemote : public RpcRemote<ConnectionLockRpcClient> {
public:
    ConnectionLockRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        ConnectionLockRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<ConnectionLockRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              client,
              timeout) {}

    folly::coro::Task<bool> lockRequest(const LockID&& lockId) {
        SLogger::trace(
            "Requesting locked connection to " +
            Identifiers::getNodeIdFromPeerDescriptor(
                this->getPeerDescriptor()));
        LockRequest request;
        request.set_lockid(lockId);

        auto options = this->formDhtRpcOptions();
        auto timeout = this->getTimeout();
        try {
            const auto res = co_await this->getClient().lockRequest(
                std::move(request), std::move(options), timeout);
            co_return res.accepted();
        } catch (const std::exception& err) {
            SLogger::debug(
                "Connection lock rejected " + std::string(err.what()));
            co_return false;
        }
    }

    folly::coro::Task<void> unlockRequest(const LockID&& lockId) {
        SLogger::trace(
            "Requesting connection to be unlocked from " +
            Identifiers::getNodeIdFromPeerDescriptor(
                this->getPeerDescriptor()));
        UnlockRequest request;
        request.set_lockid(lockId);
        auto options = this->formDhtRpcOptions();
        auto timeout = this->getTimeout();
        try {
            co_await this->getClient().unlockRequest(
                std::move(request), std::move(options), timeout);
        } catch (const std::exception& err) {
            SLogger::trace(
                "failed to send unlockRequest " + std::string(err.what()));
        }
    }

    folly::coro::Task<void> gracefulDisconnect(
        DisconnectMode&& disconnectMode) {
        SLogger::trace(
            "Notifying a graceful disconnect to " +
            Identifiers::getNodeIdFromPeerDescriptor(
                this->getPeerDescriptor()));
        DisconnectNotice request;
        request.set_disconnectmode(disconnectMode);

        DhtCallContext context;
        context.connect = false;
        context.sendIfStopped = true;
        auto timeout = std::chrono::milliseconds(2000); // NOLINT

        auto options = this->formDhtRpcOptions(context);

        co_await this->getClient().gracefulDisconnect(
            std::move(request), std::move(options), timeout);
    }
};

} // namespace streamr::dht::connection

#endif