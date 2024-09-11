// generated by the protocol buffer streamr pluging. DO NOT EDIT!
// generated from protobuf file "packages/network/protos/NetworkRpc.proto"

#ifndef STREAMR_PROTORPC_NETWORKRPC_CLIENT_PB_H
#define STREAMR_PROTORPC_NETWORKRPC_CLIENT_PB_H

#include <folly/coro/Task.h>
#include <chrono>
#include <optional>
#include "NetworkRpc.pb.h" // NOLINT
#include "streamr-proto-rpc/RpcCommunicator.hpp"


namespace streamr::protorpc {
using streamr::protorpc::RpcCommunicator;
template <typename CallContextType>
class ContentDeliveryRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit ContentDeliveryRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<void> sendStreamMessage(StreamMessage&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<StreamMessage>("sendStreamMessage", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<void> leaveStreamPartNotice(LeaveStreamPartNotice&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<LeaveStreamPartNotice>("leaveStreamPartNotice", std::move(request), std::move(callContext), timeout);
    }
}; // class ContentDeliveryRpcClient
template <typename CallContextType>
class ProxyConnectionRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit ProxyConnectionRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<ProxyConnectionResponse> requestConnection(ProxyConnectionRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<ProxyConnectionResponse, ProxyConnectionRequest>("requestConnection", std::move(request), std::move(callContext), timeout);
    }
}; // class ProxyConnectionRpcClient
template <typename CallContextType>
class HandshakeRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit HandshakeRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<StreamPartHandshakeResponse> handshake(StreamPartHandshakeRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<StreamPartHandshakeResponse, StreamPartHandshakeRequest>("handshake", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<InterleaveResponse> interleaveRequest(InterleaveRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<InterleaveResponse, InterleaveRequest>("interleaveRequest", std::move(request), std::move(callContext), timeout);
    }
}; // class HandshakeRpcClient
template <typename CallContextType>
class NeighborUpdateRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit NeighborUpdateRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<NeighborUpdate> neighborUpdate(NeighborUpdate&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<NeighborUpdate, NeighborUpdate>("neighborUpdate", std::move(request), std::move(callContext), timeout);
    }
}; // class NeighborUpdateRpcClient
template <typename CallContextType>
class TemporaryConnectionRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit TemporaryConnectionRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<TemporaryConnectionResponse> openConnection(TemporaryConnectionRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<TemporaryConnectionResponse, TemporaryConnectionRequest>("openConnection", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<void> closeConnection(CloseTemporaryConnection&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<CloseTemporaryConnection>("closeConnection", std::move(request), std::move(callContext), timeout);
    }
}; // class TemporaryConnectionRpcClient
template <typename CallContextType>
class NodeInfoRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit NodeInfoRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<NodeInfoResponse> getInfo(NodeInfoRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<NodeInfoResponse, NodeInfoRequest>("getInfo", std::move(request), std::move(callContext), timeout);
    }
}; // class NodeInfoRpcClient
}; // namespace streamr::protorpc

#endif // STREAMR_PROTORPC_NETWORKRPC_CLIENT_PB_H

