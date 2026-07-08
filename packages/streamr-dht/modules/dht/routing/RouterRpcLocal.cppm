// Module streamr.dht.RouterRpcLocal
// Ported from packages/dht/src/dht/routing/RouterRpcLocal.ts
// (v103.8.0-rc.3). The server-side handlers for the RouterRpc service:
// routeMessage (forward towards the target by distance, or deliver if this
// node is the target) and forwardMessage (forward towards a reachable-
// through peer, or deliver). Both drop duplicates via the shared detector.
module;
#include <new>


export module streamr.dht.RouterRpcLocal;

import std;

import streamr.dht.protos;

import streamr.logger.SLogger;
import streamr.utils.Uuid;
import streamr.dht.DhtRpcServer;
import streamr.dht.DhtCallContext;
import streamr.dht.DuplicateDetector;
import streamr.dht.Identifiers;
import streamr.dht.RoutingSession;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::Uuid;

export namespace streamr::dht::routing {

using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageError;
using ::dht::RouteMessageWrapper;
using streamr::dht::Identifiers;
using streamr::dht::routing::DuplicateDetector;
using streamr::dht::rpcprotocol::DhtCallContext;

using RouterRpc = ::dht::RouterRpc<DhtCallContext>;

inline RouteMessageAck createRouteMessageAck(
    const RouteMessageWrapper& routedMessage,
    std::optional<RouteMessageError> error = std::nullopt) {
    RouteMessageAck ack;
    ack.set_requestid(routedMessage.requestid());
    if (error.has_value()) {
        ack.set_error(error.value());
    }
    return ack;
}

struct RouterRpcLocalOptions {
    std::function<RouteMessageAck(
        const RouteMessageWrapper&, std::optional<RoutingMode>)>
        doRouteMessage;
    std::function<void(const RouteMessageWrapper&)> setForwardingEntries;
    std::function<void(const Message&)> handleMessage;
    DuplicateDetector& duplicateRequestDetector;
    PeerDescriptor localPeerDescriptor;
};

class RouterRpcLocal : public RouterRpc {
private:
    RouterRpcLocalOptions options;

    RouteMessageAck forwardToDestination(
        const RouteMessageWrapper& routedMessage) {
        SLogger::trace(
            "Forwarding found message targeted to self " +
            routedMessage.requestid());
        const Message& forwardedMessage = routedMessage.message();
        if (Identifiers::areEqualPeerDescriptors(
                this->options.localPeerDescriptor,
                forwardedMessage.targetdescriptor())) {
            this->options.handleMessage(forwardedMessage);
            return createRouteMessageAck(routedMessage);
        }
        RouteMessageWrapper rerouted = routedMessage;
        rerouted.set_requestid(Uuid::v4());
        rerouted.set_target(forwardedMessage.targetdescriptor().nodeid());
        return this->options.doRouteMessage(rerouted, std::nullopt);
    }

public:
    explicit RouterRpcLocal(RouterRpcLocalOptions options)
        : options(std::move(options)) {}

    ~RouterRpcLocal() override = default;

    RouteMessageAck routeMessage(
        const RouteMessageWrapper& routedMessage,
        const DhtCallContext& /*callContext*/) override {
        if (this->options.duplicateRequestDetector.isMostLikelyDuplicate(
                routedMessage.requestid())) {
            return createRouteMessageAck(
                routedMessage, RouteMessageError::DUPLICATE);
        }
        SLogger::trace(
            "Processing received routeMessage " + routedMessage.requestid());
        this->options.duplicateRequestDetector.add(routedMessage.requestid());
        if (this->options.localPeerDescriptor.nodeid() ==
            routedMessage.target()) {
            SLogger::trace(
                "routing message targeted to self " +
                routedMessage.requestid());
            this->options.setForwardingEntries(routedMessage);
            this->options.handleMessage(routedMessage.message());
            return createRouteMessageAck(routedMessage);
        }
        return this->options.doRouteMessage(routedMessage, std::nullopt);
    }

    RouteMessageAck forwardMessage(
        const RouteMessageWrapper& forwardMessage,
        const DhtCallContext& /*callContext*/) override {
        if (this->options.duplicateRequestDetector.isMostLikelyDuplicate(
                forwardMessage.requestid())) {
            return createRouteMessageAck(
                forwardMessage, RouteMessageError::DUPLICATE);
        }
        SLogger::trace(
            "Processing received forward routeMessage " +
            forwardMessage.requestid());
        this->options.duplicateRequestDetector.add(forwardMessage.requestid());
        if (this->options.localPeerDescriptor.nodeid() ==
            forwardMessage.target()) {
            return this->forwardToDestination(forwardMessage);
        }
        return this->options.doRouteMessage(
            forwardMessage, RoutingMode::FORWARD);
    }
};

} // namespace streamr::dht::routing
