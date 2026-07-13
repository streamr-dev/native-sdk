// Module streamr.trackerlessnetwork.NodeInfoRpcRemote
// Ported from packages/trackerless-network/src/NodeInfoRpcRemote.ts
// (v103.8.0-rc.3): the client-side wrapper for the NodeInfoRpc service.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <optional>
#include <utility>

export module streamr.trackerlessnetwork.NodeInfoRpcRemote;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.protos;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.RpcRemote;
import streamr.dht.protos;

using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;

export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using NodeInfoRpcClient = streamr::protorpc::NodeInfoRpcClient<DhtCallContext>;

class NodeInfoRpcRemote : public RpcRemote<NodeInfoRpcClient> {
public:
    NodeInfoRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        NodeInfoRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<NodeInfoRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              std::move(client),
              timeout) {}

    folly::coro::Task<NodeInfoResponse> getInfo() {
        auto options = this->formDhtRpcOptions({});
        co_return co_await this->getClient().getInfo(
            NodeInfoRequest{}, std::move(options));
    }
};

} // namespace streamr::trackerlessnetwork
