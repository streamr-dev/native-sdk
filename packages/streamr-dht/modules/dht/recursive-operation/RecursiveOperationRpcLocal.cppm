// Module streamr.dht.RecursiveOperationRpcLocal
// Ported from packages/dht/src/dht/recursive-operation/
// RecursiveOperationRpcLocal.ts (v103.8.0-rc.3). The server side of the
// RecursiveOperationRpc service: routeRequest forwards a recursive
// operation onward (dropping duplicates), delegating the actual routing to
// the manager's doRouteRequest.
module;


export module streamr.dht.RecursiveOperationRpcLocal;

import std;

import streamr.dht.protos;

import streamr.logger.SLogger;
import streamr.dht.DhtRpcServer;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RouterRpcLocal;
import streamr.dht.getPreviousPeer;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht::recursiveoperation {

using ::dht::PeerDescriptor;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageError;
using ::dht::RouteMessageWrapper;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::routing::createRouteMessageAck;
using streamr::dht::routing::getPreviousPeer;
using streamr::dht::rpcprotocol::DhtCallContext;

using RecursiveOperationRpc = ::dht::RecursiveOperationRpc<DhtCallContext>;

struct RecursiveOperationRpcLocalOptions {
    std::function<RouteMessageAck(const RouteMessageWrapper&)> doRouteRequest;
    std::function<void(const PeerDescriptor&, bool)> addContact;
    std::function<bool(const std::string&)> isMostLikelyDuplicate;
    std::function<void(const std::string&)> addToDuplicateDetector;
};

class RecursiveOperationRpcLocal : public RecursiveOperationRpc {
private:
    RecursiveOperationRpcLocalOptions options;

public:
    explicit RecursiveOperationRpcLocal(
        RecursiveOperationRpcLocalOptions options)
        : options(std::move(options)) {}

    ~RecursiveOperationRpcLocal() override = default;

    RouteMessageAck routeRequest(
        const RouteMessageWrapper& routedMessage,
        const DhtCallContext& /*callContext*/) override {
        if (this->options.isMostLikelyDuplicate(routedMessage.requestid())) {
            return createRouteMessageAck(
                routedMessage, RouteMessageError::DUPLICATE);
        }
        const auto previousPeer = getPreviousPeer(routedMessage);
        const DhtAddress remoteNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(
                previousPeer.has_value() ? previousPeer.value()
                                         : routedMessage.sourcepeer());
        SLogger::trace("Received routeRequest call from " + remoteNodeId);
        this->options.addToDuplicateDetector(routedMessage.requestid());
        return this->options.doRouteRequest(routedMessage);
    }
};

} // namespace streamr::dht::recursiveoperation
