// generated by the protocol buffer streamr pluging. DO NOT EDIT!
// generated from protobuf file "RoutedHelloRpc.proto"

#ifndef STREAMR_PROTORPC_ROUTEDHELLORPC_CLIENT_PB_H
#define STREAMR_PROTORPC_ROUTEDHELLORPC_CLIENT_PB_H

#include <folly/experimental/coro/Task.h>
#include "RoutedHelloRpc.pb.h" // NOLINT
#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"


namespace streamr::protorpc {
class RoutedHelloRpcServiceClient {
private:
RpcCommunicator& communicator;
public:
    RoutedHelloRpcServiceClient(RpcCommunicator& communicator) : communicator(communicator) {}
    folly::coro::Task<RoutedHelloResponse> sayHello(const RoutedHelloRequest& request, const ProtoCallContext& callContext) {
        return communicator.request<RoutedHelloResponse, RoutedHelloRequest>("sayHello", request, callContext);
    }
}; // class RoutedHelloRpcServiceClient
}; // namespace streamr::protorpc

#endif // STREAMR_PROTORPC_ROUTEDHELLORPC_CLIENT_PB_H
