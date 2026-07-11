// Module streamr.trackerlessnetwork.TemporaryConnectionRpcRemote
// Ported from packages/trackerless-network/src/content-delivery-layer/
// temporary-connection/TemporaryConnectionRpcRemote.ts (v103.8.0-rc.3):
// the client side of temporary content-delivery connections. Both RPCs
// swallow errors (open reports failure as `false`, close is
// fire-and-forget), matching the TS behavior.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <string>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.TemporaryConnectionRpcRemote;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RpcRemote;
import streamr.dht.protos;
import streamr.logger.SLogger;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using streamr::dht::Identifiers;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::logger::SLogger;

export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using TemporaryConnectionRpcClient =
    streamr::protorpc::TemporaryConnectionRpcClient<DhtCallContext>;

class TemporaryConnectionRpcRemote
    : public RpcRemote<TemporaryConnectionRpcClient> {
public:
    TemporaryConnectionRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        TemporaryConnectionRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<TemporaryConnectionRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              client,
              timeout) {}

    folly::coro::Task<bool> openConnection() {
        TemporaryConnectionRequest request;
        auto options = this->formDhtRpcOptions({});
        try {
            const auto response = co_await this->getClient().openConnection(
                std::move(request), std::move(options));
            co_return response.accepted();
        } catch (const std::exception& err) {
            SLogger::debug(
                "temporaryConnection to " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->getPeerDescriptor()) +
                " failed: " + std::string(err.what()));
            co_return false;
        }
    }

    folly::coro::Task<void> closeConnection() {
        CloseTemporaryConnection request;
        auto options = this->formDhtRpcOptions({});
        try {
            co_await this->getClient().closeConnection(
                std::move(request), std::move(options));
        } catch (const std::exception& err) {
            SLogger::trace(
                "closeConnection to " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->getPeerDescriptor()) +
                " failed: " + std::string(err.what()));
        }
    }
};

} // namespace streamr::trackerlessnetwork
