// Module streamr.trackerlessnetwork.NodeInfoRpcLocal
// Ported from packages/trackerless-network/src/NodeInfoRpcLocal.ts
// (v103.8.0-rc.3): serves getInfo over the node-info RPC service.
// The TS class holds the NetworkStack and calls stack.createNodeInfo();
// here the response factory is injected as a std::function so this
// module does not import the NetworkStack module (which imports this
// one).
module;

#include <functional>
#include <string>
#include <utility>

export module streamr.trackerlessnetwork.NodeInfoRpcLocal;

import streamr.trackerlessnetwork.protos;
import streamr.trackerlessnetwork.NetworkRpcServer;
import streamr.dht.DhtCallContext;
import streamr.dht.ListeningRpcCommunicator;

using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;

export namespace streamr::trackerlessnetwork {

// TS NODE_INFO_RPC_SERVICE_ID.
inline constexpr auto nodeInfoRpcServiceId = "system/node-info-rpc";

class NodeInfoRpcLocal : public streamr::protorpc::NodeInfoRpc<DhtCallContext> {
private:
    std::function<NodeInfoResponse()> createNodeInfo;
    ListeningRpcCommunicator& rpcCommunicator;

public:
    NodeInfoRpcLocal(
        std::function<NodeInfoResponse()> createNodeInfo, // NOLINT
        ListeningRpcCommunicator& rpcCommunicator)
        : createNodeInfo(std::move(createNodeInfo)),
          rpcCommunicator(rpcCommunicator) {
        this->rpcCommunicator
            .registerRpcMethod<NodeInfoRequest, NodeInfoResponse>(
                "getInfo",
                [this](
                    const NodeInfoRequest& request,
                    const DhtCallContext& context) {
                    return this->getInfo(request, context);
                });
    }

    NodeInfoResponse getInfo(
        const NodeInfoRequest& /* request */,
        const DhtCallContext& /* callContext */) override {
        return this->createNodeInfo();
    }
};

} // namespace streamr::trackerlessnetwork
