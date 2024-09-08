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
template <typename CallContextType>
class WakeUpRpcServiceClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit WakeUpRpcServiceClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<void> wakeUp(WakeUpRequest&& request, CallContextType&& callContext) {
        return communicator.template notify<WakeUpRequest>("wakeUp", std::move(request), std::move(callContext));
    }
}; // class WakeUpRpcServiceClient
}; // namespace streamr::protorpc

#endif // STREAMR_PROTORPC_WAKEUPRPC_CLIENT_PB_H

