// Generated by the protocol buffer streamr pluging. DO NOT EDIT!
// Generated from protobuf file "TestProtos.proto"

#ifndef STREAMR_PROTORPC_TESTPROTOS_SERVER_PB_H
#define STREAMR_PROTORPC_TESTPROTOS_SERVER_PB_H

#include "TestProtos.pb.h" // NOLINT
#include <folly/experimental/coro/Task.h>

namespace streamr::protorpc {
template <typename CallContextType>
class DhtRpcService {
public:
   virtual ~DhtRpcService() = default;
   virtual ClosestPeersResponse getClosestPeers(const ClosestPeersRequest& request, const CallContextType& callContext) = 0;
   virtual PingResponse ping(const PingRequest& request, const CallContextType& callContext) = 0;
   virtual RouteMessageAck routeMessage(const RouteMessageWrapper& request, const CallContextType& callContext) = 0;
}; // class DhtRpcService
template <typename CallContextType>
class OptionalService {
public:
   virtual ~OptionalService() = default;
   virtual OptionalResponse getOptional(const OptionalRequest& request, const CallContextType& callContext) = 0;
}; // class OptionalService
}; // namespace streamr::protorpc

#endif // STREAMR_PROTORPC_TESTPROTOS_SERVER_PB_H

