// Generated by the protocol buffer streamr pluging. DO NOT EDIT!
// Generated from protobuf file "packages/network/protos/NetworkRpc.proto"

#ifndef STREAMR_PROTORPC_NETWORKRPC_SERVER_PB_H
#define STREAMR_PROTORPC_NETWORKRPC_SERVER_PB_H

#include "NetworkRpc.pb.h" // NOLINT
#include <folly/coro/Task.h>

namespace streamr::protorpc {
template <typename CallContextType>
class ContentDeliveryRpc {
public:
   virtual ~ContentDeliveryRpc() = default;
   virtual void sendStreamMessage(const StreamMessage& request, const CallContextType& callContext) = 0;
   virtual void leaveStreamPartNotice(const LeaveStreamPartNotice& request, const CallContextType& callContext) = 0;
}; // class ContentDeliveryRpc
template <typename CallContextType>
class ProxyConnectionRpc {
public:
   virtual ~ProxyConnectionRpc() = default;
   virtual ProxyConnectionResponse requestConnection(const ProxyConnectionRequest& request, const CallContextType& callContext) = 0;
}; // class ProxyConnectionRpc
template <typename CallContextType>
class HandshakeRpc {
public:
   virtual ~HandshakeRpc() = default;
   virtual StreamPartHandshakeResponse handshake(const StreamPartHandshakeRequest& request, const CallContextType& callContext) = 0;
   virtual InterleaveResponse interleaveRequest(const InterleaveRequest& request, const CallContextType& callContext) = 0;
}; // class HandshakeRpc
template <typename CallContextType>
class NeighborUpdateRpc {
public:
   virtual ~NeighborUpdateRpc() = default;
   virtual NeighborUpdate neighborUpdate(const NeighborUpdate& request, const CallContextType& callContext) = 0;
}; // class NeighborUpdateRpc
template <typename CallContextType>
class TemporaryConnectionRpc {
public:
   virtual ~TemporaryConnectionRpc() = default;
   virtual TemporaryConnectionResponse openConnection(const TemporaryConnectionRequest& request, const CallContextType& callContext) = 0;
   virtual void closeConnection(const CloseTemporaryConnection& request, const CallContextType& callContext) = 0;
}; // class TemporaryConnectionRpc
template <typename CallContextType>
class NodeInfoRpc {
public:
   virtual ~NodeInfoRpc() = default;
   virtual NodeInfoResponse getInfo(const NodeInfoRequest& request, const CallContextType& callContext) = 0;
}; // class NodeInfoRpc
}; // namespace streamr::protorpc

#endif // STREAMR_PROTORPC_NETWORKRPC_SERVER_PB_H
