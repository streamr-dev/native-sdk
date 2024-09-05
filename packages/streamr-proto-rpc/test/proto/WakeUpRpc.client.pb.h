// generated by the protocol buffer streamr pluging. DO NOT EDIT!
// generated from protobuf file "WakeUpRpc.proto"

#ifndef STREAMR_PROTORPC_WAKEUPRPC_CLIENT_PB_H
#define STREAMR_PROTORPC_WAKEUPRPC_CLIENT_PB_H

#include <folly/experimental/coro/Task.h>
#include "WakeUpRpc.pb.h" // NOLINT
#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"


namespace streamr::protorpc {
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::ProtoCallContext;
class WakeUpRpcServiceClient {
private:
RpcCommunicator& communicator;
public:
    explicit WakeUpRpcServiceClient(RpcCommunicator& communicator) : communicator(communicator) {}
    folly::coro::Task<void> wakeUp(const WakeUpRequest& request, const ProtoCallContext& callContext) {
        return communicator.notify<WakeUpRequest>("wakeUp", request, callContext);
    }
}; // class WakeUpRpcServiceClient
}; // namespace streamr::protorpc

#endif // STREAMR_PROTORPC_WAKEUPRPC_CLIENT_PB_H

