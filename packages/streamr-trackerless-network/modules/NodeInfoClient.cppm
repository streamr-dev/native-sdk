// Module streamr.trackerlessnetwork.NodeInfoClient
// Ported from packages/trackerless-network/src/NodeInfoClient.ts
// (v103.8.0-rc.3): fetches NodeInfo from a remote node over the
// node-info RPC service.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <utility>

export module streamr.trackerlessnetwork.NodeInfoClient;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.protos;
import streamr.trackerlessnetwork.NodeInfoRpcRemote;
import streamr.dht.DhtCallContext;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;

using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;

export namespace streamr::trackerlessnetwork {

class NodeInfoClient {
private:
    PeerDescriptor ownPeerDescriptor;
    ListeningRpcCommunicator& rpcCommunicator;

public:
    NodeInfoClient(
        PeerDescriptor ownPeerDescriptor, // NOLINT
        ListeningRpcCommunicator& rpcCommunicator)
        : ownPeerDescriptor(std::move(ownPeerDescriptor)),
          rpcCommunicator(rpcCommunicator) {}

    folly::coro::Task<NodeInfoResponse> getInfo(PeerDescriptor node) {
        NodeInfoRpcClient client{this->rpcCommunicator};
        NodeInfoRpcRemote remote(
            this->ownPeerDescriptor, std::move(node), std::move(client));
        co_return co_await remote.getInfo();
    }
};

} // namespace streamr::trackerlessnetwork
