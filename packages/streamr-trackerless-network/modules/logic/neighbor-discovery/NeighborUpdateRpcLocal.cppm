// Module streamr.trackerlessnetwork.NeighborUpdateRpcLocal
// Ported from packages/trackerless-network/src/content-delivery-layer/
// neighbor-discovery/NeighborUpdateRpcLocal.ts (v103.8.0-rc.3): the
// server side of periodic neighbor-list exchange — learns new contacts
// from the caller's neighbor list and asks to be dropped when both sides
// have more neighbors than the target count.
module;

#include <algorithm>
#include <memory>
#include <set>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.NeighborUpdateRpcLocal;

import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.NeighborFinder;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.NodeList;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::trackerlessnetwork::ContentDeliveryRpcClient;
using streamr::trackerlessnetwork::ContentDeliveryRpcRemote;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork::neighbordiscovery {

using ::dht::PeerDescriptor;

struct NeighborUpdateRpcLocalOptions {
    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    NodeList& neighbors;
    NodeList& nearbyNodeView;
    INeighborFinder& neighborFinder;
    ListeningRpcCommunicator& rpcCommunicator;
    size_t neighborTargetCount;
    std::set<DhtAddress>& ongoingHandshakes;
};

class NeighborUpdateRpcLocal {
private:
    NeighborUpdateRpcLocalOptions options;

public:
    explicit NeighborUpdateRpcLocal(NeighborUpdateRpcLocalOptions options)
        : options(std::move(options)) {}

    NeighborUpdate neighborUpdate(
        const NeighborUpdate& message, const DhtCallContext& context) {
        const auto senderPeerDescriptor =
            context.incomingSourceDescriptor.value();
        const auto remoteNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(senderPeerDescriptor);
        this->updateContacts(message.neighbordescriptors());
        if (!this->options.neighbors.has(remoteNodeId) &&
            !this->options.ongoingHandshakes.contains(remoteNodeId)) {
            return this->createResponse(true);
        }
        const auto isOverNeighborCount = this->options.neighbors.size() >
                this->options.neighborTargetCount &&
            // Motivation: We don't know the remote's neighborTargetCount
            // setting here. We only ask to cut connections if the remote
            // has a "sufficient" number of neighbors, where "sufficient"
            // means our neighborTargetCount setting.
            static_cast<size_t>(message.neighbordescriptors().size()) >
                this->options.neighborTargetCount;
        if (!isOverNeighborCount) {
            this->options.neighborFinder.start();
        } else {
            this->options.neighbors.remove(remoteNodeId);
        }
        return this->createResponse(isOverNeighborCount);
    }

private:
    template <typename DescriptorRange>
    void updateContacts(const DescriptorRange& neighborDescriptors) {
        const auto ownNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            this->options.localPeerDescriptor);
        const auto knownIds = this->options.neighbors.getIds();
        for (const auto& peerDescriptor : neighborDescriptors) {
            const auto nodeId =
                Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
            if (nodeId != ownNodeId &&
                std::ranges::find(knownIds, nodeId) == knownIds.end()) {
                ContentDeliveryRpcClient client{this->options.rpcCommunicator};
                this->options.nearbyNodeView.add(
                    std::make_shared<ContentDeliveryRpcRemote>(
                        this->options.localPeerDescriptor,
                        peerDescriptor,
                        client));
            }
        }
    }

    NeighborUpdate createResponse(bool removeMe) const {
        NeighborUpdate response;
        response.set_streampartid(this->options.streamPartId);
        for (const auto& neighbor : this->options.neighbors.getAll()) {
            *response.add_neighbordescriptors() = neighbor->getPeerDescriptor();
        }
        response.set_removeme(removeMe);
        return response;
    }
};

} // namespace streamr::trackerlessnetwork::neighbordiscovery
