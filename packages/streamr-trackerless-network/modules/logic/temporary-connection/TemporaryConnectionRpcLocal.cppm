// Module streamr.trackerlessnetwork.TemporaryConnectionRpcLocal
// Ported from packages/trackerless-network/src/content-delivery-layer/
// temporary-connection/TemporaryConnectionRpcLocal.ts (v103.8.0-rc.3):
// the server side of temporary content-delivery connections (used by
// proxy/inspection flows), tracking the requesting nodes in a NodeList
// and weak-locking the underlying connection while it is in use.
module;

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

export module streamr.trackerlessnetwork.TemporaryConnectionRpcLocal;

import streamr.trackerlessnetwork.protos;

import streamr.trackerlessnetwork.NetworkRpcServer;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.NodeList;
import streamr.dht.ConnectionLocker;
import streamr.dht.ConnectionLockStates;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

// Hoisted from the former header style (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::LockID;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using TemporaryConnectionRpc =
    streamr::protorpc::TemporaryConnectionRpc<DhtCallContext>;
using ContentDeliveryRpcClient =
    streamr::protorpc::ContentDeliveryRpcClient<DhtCallContext>;

struct TemporaryConnectionRpcLocalOptions {
    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    ListeningRpcCommunicator& rpcCommunicator;
    ConnectionLocker& connectionLocker;
};

constexpr auto temporaryConnectionLockIdBase =
    "system/content-delivery/temporary-connection/";

class TemporaryConnectionRpcLocal : public TemporaryConnectionRpc {
private:
    // TS TODO preserved: use an options option or a named constant?
    static constexpr size_t temporaryNodesLimit = 10;

    TemporaryConnectionRpcLocalOptions options;
    NodeList temporaryNodes;
    LockID lockId;

public:
    explicit TemporaryConnectionRpcLocal(
        TemporaryConnectionRpcLocalOptions options)
        : options(std::move(options)),
          temporaryNodes(
              Identifiers::getNodeIdFromPeerDescriptor(
                  this->options.localPeerDescriptor),
              temporaryNodesLimit),
          lockId(
              LockID{
                  temporaryConnectionLockIdBase + this->options.streamPartId}) {
    }

    [[nodiscard]] NodeList& getNodes() { return this->temporaryNodes; }

    [[nodiscard]] bool hasNode(const DhtAddress& node) const {
        return this->temporaryNodes.has(node);
    }

    void removeNode(const DhtAddress& nodeId) {
        this->temporaryNodes.remove(nodeId);
        this->options.connectionLocker.weakUnlockConnection(
            nodeId, this->lockId);
    }

    TemporaryConnectionResponse openConnection(
        const TemporaryConnectionRequest& /*request*/,
        const DhtCallContext& context) override {
        const auto& sender = context.incomingSourceDescriptor.value();
        ContentDeliveryRpcClient client{this->options.rpcCommunicator};
        const auto remote = std::make_shared<ContentDeliveryRpcRemote>(
            this->options.localPeerDescriptor, sender, client);
        this->temporaryNodes.add(remote);
        this->options.connectionLocker.weakLockConnection(
            Identifiers::getNodeIdFromPeerDescriptor(sender), this->lockId);
        TemporaryConnectionResponse response;
        response.set_accepted(true);
        return response;
    }

    void closeConnection(
        const CloseTemporaryConnection& /*request*/,
        const DhtCallContext& context) override {
        const auto remoteNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            context.incomingSourceDescriptor.value());
        this->removeNode(remoteNodeId);
    }
};

} // namespace streamr::trackerlessnetwork
