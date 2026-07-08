// Module streamr.dht.RoutingRemoteContact
// Ported from the RoutingRemoteContact class in
// packages/dht/src/dht/routing/RoutingSession.ts (v103.8.0-rc.3).
//
// Extracted into its own module (TS keeps it inside RoutingSession.ts):
// RoutingTablesCache holds RoutingRemoteContact and RoutingSession holds
// RoutingTablesCache, which is a circular import TS tolerates but C++
// modules cannot. Giving RoutingRemoteContact its own module breaks the
// cycle.
module;

export module streamr.dht.RoutingRemoteContact;

import streamr.dht.protos;

import streamr.protorpc.RpcCommunicator;
import streamr.dht.Contact;
import streamr.dht.DhtCallContext;
import streamr.dht.RouterRpcRemote;
import streamr.dht.RecursiveOperationRpcRemote;

export namespace streamr::dht::routing {

using ::dht::PeerDescriptor;
using streamr::dht::contact::Contact;
using streamr::dht::recursiveoperation::RecursiveOperationRpcClient;
using streamr::dht::recursiveoperation::RecursiveOperationRpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::protorpc::RpcCommunicator;
// RouterRpcRemote, RouterRpcClient and routingTimeout are visible from the
// imported streamr.dht.RouterRpcRemote (same namespace).

class RoutingRemoteContact : public Contact {
private:
    RouterRpcRemote routerRpcRemote;
    RecursiveOperationRpcRemote recursiveOperationRpcRemote;

public:
    RoutingRemoteContact(
        const PeerDescriptor& peer,
        const PeerDescriptor& localPeerDescriptor,
        RpcCommunicator<DhtCallContext>& rpcCommunicator)
        : Contact(peer),
          routerRpcRemote(
              localPeerDescriptor,
              peer,
              RouterRpcClient(rpcCommunicator),
              routingTimeout),
          recursiveOperationRpcRemote(
              localPeerDescriptor,
              peer,
              RecursiveOperationRpcClient(rpcCommunicator),
              routingTimeout) {}

    [[nodiscard]] RouterRpcRemote& getRouterRpcRemote() {
        return this->routerRpcRemote;
    }

    [[nodiscard]] RecursiveOperationRpcRemote&
    getRecursiveOperationRpcRemote() {
        return this->recursiveOperationRpcRemote;
    }
};

} // namespace streamr::dht::routing
