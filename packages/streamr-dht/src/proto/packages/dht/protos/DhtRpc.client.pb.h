// generated by the protocol buffer streamr pluging. DO NOT EDIT!
// generated from protobuf file "packages/dht/protos/DhtRpc.proto"

#ifndef STREAMR_PROTORPC_DHTRPC_CLIENT_PB_H
#define STREAMR_PROTORPC_DHTRPC_CLIENT_PB_H

#include <folly/experimental/coro/Task.h>
#include <chrono>
#include <optional>
#include "DhtRpc.pb.h" // NOLINT
#include "streamr-proto-rpc/RpcCommunicator.hpp"


namespace dht {
using streamr::protorpc::RpcCommunicator;
template <typename CallContextType>
class DhtNodeRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit DhtNodeRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<ClosestPeersResponse> getClosestPeers(ClosestPeersRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<ClosestPeersResponse, ClosestPeersRequest>("getClosestPeers", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<ClosestRingPeersResponse> getClosestRingPeers(ClosestRingPeersRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<ClosestRingPeersResponse, ClosestRingPeersRequest>("getClosestRingPeers", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<PingResponse> ping(PingRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<PingResponse, PingRequest>("ping", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<void> leaveNotice(LeaveNotice&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<LeaveNotice>("leaveNotice", std::move(request), std::move(callContext), timeout);
    }
}; // class DhtNodeRpcClient
template <typename CallContextType>
class RouterRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit RouterRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<RouteMessageAck> routeMessage(RouteMessageWrapper&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<RouteMessageAck, RouteMessageWrapper>("routeMessage", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<RouteMessageAck> forwardMessage(RouteMessageWrapper&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<RouteMessageAck, RouteMessageWrapper>("forwardMessage", std::move(request), std::move(callContext), timeout);
    }
}; // class RouterRpcClient
template <typename CallContextType>
class RecursiveOperationRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit RecursiveOperationRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<RouteMessageAck> routeRequest(RouteMessageWrapper&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<RouteMessageAck, RouteMessageWrapper>("routeRequest", std::move(request), std::move(callContext), timeout);
    }
}; // class RecursiveOperationRpcClient
template <typename CallContextType>
class StoreRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit StoreRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<StoreDataResponse> storeData(StoreDataRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<StoreDataResponse, StoreDataRequest>("storeData", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<void> replicateData(ReplicateDataRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<ReplicateDataRequest>("replicateData", std::move(request), std::move(callContext), timeout);
    }
}; // class StoreRpcClient
template <typename CallContextType>
class RecursiveOperationSessionRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit RecursiveOperationSessionRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<void> sendResponse(RecursiveOperationResponse&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<RecursiveOperationResponse>("sendResponse", std::move(request), std::move(callContext), timeout);
    }
}; // class RecursiveOperationSessionRpcClient
template <typename CallContextType>
class WebsocketClientConnectorRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit WebsocketClientConnectorRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<void> requestConnection(WebsocketConnectionRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<WebsocketConnectionRequest>("requestConnection", std::move(request), std::move(callContext), timeout);
    }
}; // class WebsocketClientConnectorRpcClient
template <typename CallContextType>
class WebrtcConnectorRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit WebrtcConnectorRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<void> requestConnection(WebrtcConnectionRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<WebrtcConnectionRequest>("requestConnection", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<void> rtcOffer(RtcOffer&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<RtcOffer>("rtcOffer", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<void> rtcAnswer(RtcAnswer&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<RtcAnswer>("rtcAnswer", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<void> iceCandidate(IceCandidate&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<IceCandidate>("iceCandidate", std::move(request), std::move(callContext), timeout);
    }
}; // class WebrtcConnectorRpcClient
template <typename CallContextType>
class ConnectionLockRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit ConnectionLockRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<LockResponse> lockRequest(LockRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<LockResponse, LockRequest>("lockRequest", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<void> unlockRequest(UnlockRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<UnlockRequest>("unlockRequest", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<void> gracefulDisconnect(DisconnectNotice&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template notify<DisconnectNotice>("gracefulDisconnect", std::move(request), std::move(callContext), timeout);
    }
}; // class ConnectionLockRpcClient
template <typename CallContextType>
class ExternalApiRpcClient {
private:
RpcCommunicator<CallContextType>& communicator;
public:
    explicit ExternalApiRpcClient(RpcCommunicator<CallContextType>& communicator) : communicator(communicator) {}
    folly::coro::Task<ExternalFetchDataResponse> externalFetchData(ExternalFetchDataRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<ExternalFetchDataResponse, ExternalFetchDataRequest>("externalFetchData", std::move(request), std::move(callContext), timeout);
    }
    folly::coro::Task<ExternalStoreDataResponse> externalStoreData(ExternalStoreDataRequest&& request, CallContextType&& callContext, std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return communicator.template request<ExternalStoreDataResponse, ExternalStoreDataRequest>("externalStoreData", std::move(request), std::move(callContext), timeout);
    }
}; // class ExternalApiRpcClient
}; // namespace dht

#endif // STREAMR_PROTORPC_DHTRPC_CLIENT_PB_H

