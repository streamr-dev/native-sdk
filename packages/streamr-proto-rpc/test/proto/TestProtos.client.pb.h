// generated by the protocol buffer streamr pluging. DO NOT EDIT!
// generated from protobuf file "TestProtos.proto"

#ifndef STREAMR_PROTORPC_TESTPROTOS_CLIENT_PB_H
#define STREAMR_PROTORPC_TESTPROTOS_CLIENT_PB_H

#include <folly/experimental/coro/Task.h>
#include "TestProtos.pb.h" // NOLINT
#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"


namespace streamr::protorpc {
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::ProtoCallContext;
template <typename CallContextType>
class DhtRpcServiceClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit DhtRpcServiceClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<ClosestPeersResponse> getClosestPeers(ClosestPeersRequest&& request, CallContextType&& callContext) {
        return communicator.template request<ClosestPeersResponse, ClosestPeersRequest>("getClosestPeers", std::move(request), std::move(callContext));
    }
    folly::coro::Task<PingResponse> ping(PingRequest&& request, CallContextType&& callContext) {
        return communicator.template request<PingResponse, PingRequest>("ping", std::move(request), std::move(callContext));
    }
    folly::coro::Task<RouteMessageAck> routeMessage(RouteMessageWrapper&& request, CallContextType&& callContext) {
        return communicator.template request<RouteMessageAck, RouteMessageWrapper>("routeMessage", std::move(request), std::move(callContext));
    }
}; // class DhtRpcServiceClient
template <typename CallContextType>
class OptionalServiceClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit OptionalServiceClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<OptionalResponse> getOptional(OptionalRequest&& request, CallContextType&& callContext) {
        return communicator.template request<OptionalResponse, OptionalRequest>("getOptional", std::move(request), std::move(callContext));
    }
}; // class OptionalServiceClient
}; // namespace streamr::protorpc

#endif // STREAMR_PROTORPC_TESTPROTOS_CLIENT_PB_H

