// Module streamr.dht.RecursiveOperationRpcRemote
// Ported from packages/dht/src/dht/recursive-operation/
// RecursiveOperationRpcRemote.ts (v103.8.0-rc.3).
//
// Pulled forward into phase A4 (the rest of the recursive-operation cluster
// lands in A5): RoutingSession's RoutingRemoteContact constructs one of
// these for every contact, so RoutingSession cannot compile without it.
// Only the routeRequest client call is needed here.
module;

#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <optional>
#include <string>
#include <utility>

export module streamr.dht.RecursiveOperationRpcRemote;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.utils.Uuid;
import streamr.logger.SLogger;
import streamr.dht.DhtRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RpcRemote;
import streamr.dht.getPreviousPeer;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::Uuid;

export namespace streamr::dht::recursiveoperation {

using ::dht::PeerDescriptor;
using ::dht::RouteMessageWrapper;
using streamr::dht::Identifiers;
using streamr::dht::contact::RpcRemote;
using streamr::dht::routing::getPreviousPeer;
using streamr::dht::rpcprotocol::DhtCallContext;

using RecursiveOperationRpcClient =
    ::dht::RecursiveOperationRpcClient<DhtCallContext>;

class RecursiveOperationRpcRemote
    : public RpcRemote<RecursiveOperationRpcClient> {
public:
    RecursiveOperationRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        RecursiveOperationRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<RecursiveOperationRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              std::move(client),
              timeout) {}

    folly::coro::Task<bool> routeRequest(const RouteMessageWrapper& params) {
        RouteMessageWrapper message = params;
        if (message.requestid().empty()) {
            message.set_requestid(Uuid::v4());
        }
        DhtCallContext context;
        context.connect = false;
        auto options = this->formDhtRpcOptions(context);
        auto timeout = this->getTimeout();
        try {
            const auto ack = co_await this->getClient().routeRequest(
                std::move(message), std::move(options), timeout);
            if (ack.has_error()) {
                SLogger::trace("Next hop responded with error");
                co_return false;
            }
        } catch (const std::exception& err) {
            const auto previousPeer = getPreviousPeer(params);
            const DhtAddress fromNode = previousPeer.has_value()
                ? Identifiers::getNodeIdFromPeerDescriptor(previousPeer.value())
                : Identifiers::getNodeIdFromPeerDescriptor(params.sourcepeer());
            const DhtAddress toNode = Identifiers::getNodeIdFromPeerDescriptor(
                this->getPeerDescriptor());
            SLogger::debug(
                "Failed to send routeRequest message from " + fromNode +
                " to " + toNode + " " + std::string(err.what()));
            co_return false;
        }
        co_return true;
    }
};

} // namespace streamr::dht::recursiveoperation
