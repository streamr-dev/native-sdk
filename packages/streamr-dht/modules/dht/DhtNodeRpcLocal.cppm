// Module streamr.dht.DhtNodeRpcLocal
// Ported from packages/dht/src/dht/DhtNodeRpcLocal.ts (v103.8.0-rc.3).
// The server-side handlers for the DhtNodeRpc service (peer discovery and
// liveness): getClosestPeers, getClosestRingPeers, ping, leaveNotice.
module;

#include <cstddef>
#include <functional>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.DhtNodeRpcLocal;

import streamr.logger.SLogger;
import streamr.dht.DhtRpcServer;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.getClosestNodes;
import streamr.dht.Identifiers;
import streamr.dht.ringIdentifiers;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht {

using ::dht::ClosestPeersRequest;
using ::dht::ClosestPeersResponse;
using ::dht::ClosestRingPeersRequest;
using ::dht::ClosestRingPeersResponse;
using ::dht::LeaveNotice;
using ::dht::PeerDescriptor;
using ::dht::PingRequest;
using ::dht::PingResponse;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::contact::GetClosestNodesOptions;
using streamr::dht::contact::RingIdRaw;
using streamr::dht::rpcprotocol::DhtCallContext;

using DhtNodeRpc = ::dht::DhtNodeRpc<DhtCallContext>;

struct DhtNodeRpcLocalOptions {
    size_t peerDiscoveryQueryBatchSize;
    std::function<std::vector<PeerDescriptor>()> getNeighbors;
    std::function<ClosestRingPeerDescriptors(const RingIdRaw&, size_t)>
        getClosestRingContactsTo;
    std::function<void(const PeerDescriptor&)> addContact;
    std::function<void(const DhtAddress&)> removeContact;
};

class DhtNodeRpcLocal : public DhtNodeRpc {
private:
    DhtNodeRpcLocalOptions options;

public:
    explicit DhtNodeRpcLocal(DhtNodeRpcLocalOptions options)
        : options(std::move(options)) {}

    ~DhtNodeRpcLocal() override = default;

    ClosestPeersResponse getClosestPeers(
        const ClosestPeersRequest& request,
        const DhtCallContext& callContext) override {
        this->options.addContact(callContext.incomingSourceDescriptor.value());
        const auto peers = getClosestNodes(
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{request.nodeid()}),
            this->options.getNeighbors(),
            GetClosestNodesOptions{
                .maxCount = this->options.peerDiscoveryQueryBatchSize});
        ClosestPeersResponse response;
        for (const auto& peer : peers) {
            *response.add_peers() = peer;
        }
        response.set_requestid(request.requestid());
        return response;
    }

    ClosestRingPeersResponse getClosestRingPeers(
        const ClosestRingPeersRequest& request,
        const DhtCallContext& callContext) override {
        this->options.addContact(callContext.incomingSourceDescriptor.value());
        const auto closest = this->options.getClosestRingContactsTo(
            RingIdRaw{request.ringid()},
            this->options.peerDiscoveryQueryBatchSize);
        ClosestRingPeersResponse response;
        for (const auto& peer : closest.left) {
            *response.add_leftpeers() = peer;
        }
        for (const auto& peer : closest.right) {
            *response.add_rightpeers() = peer;
        }
        response.set_requestid(request.requestid());
        return response;
    }

    PingResponse ping(
        const PingRequest& request,
        const DhtCallContext& callContext) override {
        SLogger::trace(
            "received ping request from " +
            Identifiers::getNodeIdFromPeerDescriptor(
                callContext.incomingSourceDescriptor.value()));
        this->options.addContact(callContext.incomingSourceDescriptor.value());
        PingResponse response;
        response.set_requestid(request.requestid());
        return response;
    }

    void leaveNotice(
        const LeaveNotice& /*request*/,
        const DhtCallContext& callContext) override {
        const auto sender = callContext.incomingSourceDescriptor.value();
        SLogger::trace(
            "received leave notice from " +
            Identifiers::getNodeIdFromPeerDescriptor(sender));
        this->options.removeContact(
            Identifiers::getNodeIdFromPeerDescriptor(sender));
    }
};

} // namespace streamr::dht
