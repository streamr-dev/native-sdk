// Module streamr.dht.RouterRpcRemote
// Ported from packages/dht/src/dht/routing/RouterRpcRemote.ts
// (v103.8.0-rc.3). The client-side wrapper for a peer's RouterRpc service:
// routeMessage (forward towards the target by distance) and forwardMessage
// (forward towards a known reachable-through peer).
module;
#include <new> // operator new ambiguity under import std (local-type container allocation) — see convert-to-import-std.py

// std::coroutine_traits must be visible in the TU that defines a coroutine.


export module streamr.dht.RouterRpcRemote;

import std;

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

export namespace streamr::dht::routing {

using ::dht::PeerDescriptor;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageError;
using ::dht::RouteMessageWrapper;
using streamr::dht::Identifiers;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;

using RouterRpcClient = ::dht::RouterRpcClient<DhtCallContext>;

// default routing RPC timeout
inline constexpr std::chrono::milliseconds routingTimeout{2000};

class RouterRpcRemote : public RpcRemote<RouterRpcClient> {
public:
    RouterRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        RouterRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<RouterRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              std::move(client),
              timeout) {}

    folly::coro::Task<bool> routeMessage(const RouteMessageWrapper& params) {
        RouteMessageWrapper message = params;
        if (message.requestid().empty()) {
            message.set_requestid(Uuid::v4());
        }
        DhtCallContext context;
        context.connect = false;
        auto options = this->formDhtRpcOptions(context);
        auto timeout = this->getTimeout();
        try {
            const auto ack = co_await this->getClient().routeMessage(
                std::move(message), std::move(options), timeout);
            // Success signal if sent to destination and error is a duplicate.
            if (ack.has_error() &&
                ack.error() == RouteMessageError::DUPLICATE &&
                params.target() == this->getPeerDescriptor().nodeid()) {
                co_return true;
            }
            if (ack.has_error()) {
                co_return false;
            }
        } catch (const std::exception& err) {
            this->logSendFailure("routeMessage", params, err);
            co_return false;
        }
        co_return true;
    }

    folly::coro::Task<bool> forwardMessage(const RouteMessageWrapper& params) {
        RouteMessageWrapper message = params;
        if (message.requestid().empty()) {
            message.set_requestid(Uuid::v4());
        }
        DhtCallContext context;
        context.connect = false;
        auto options = this->formDhtRpcOptions(context);
        auto timeout = this->getTimeout();
        try {
            const auto ack = co_await this->getClient().forwardMessage(
                std::move(message), std::move(options), timeout);
            if (ack.has_error()) {
                co_return false;
            }
        } catch (const std::exception& err) {
            this->logSendFailure("forwardMessage", params, err);
            co_return false;
        }
        co_return true;
    }

private:
    void logSendFailure(
        const std::string& method,
        const RouteMessageWrapper& params,
        const std::exception& err) {
        const auto previousPeer = getPreviousPeer(params);
        const DhtAddress fromNode = previousPeer.has_value()
            ? Identifiers::getNodeIdFromPeerDescriptor(previousPeer.value())
            : Identifiers::getNodeIdFromPeerDescriptor(params.sourcepeer());
        const DhtAddress toNode =
            Identifiers::getNodeIdFromPeerDescriptor(this->getPeerDescriptor());
        SLogger::trace(
            "Failed to send " + method + " from " + fromNode + " to " + toNode +
            " " + std::string(err.what()));
    }
};

} // namespace streamr::dht::routing
