// Generated by the protocol buffer streamr pluging. DO NOT EDIT!
// Generated from protobuf file "RoutedHelloRpc.proto"

#ifndef STREAMR_PROTORPC_ROUTEDHELLORPC_SERVER_PB_H
#define STREAMR_PROTORPC_ROUTEDHELLORPC_SERVER_PB_H

#include "RoutedHelloRpc.pb.h" // NOLINT 
#include <folly/experimental/coro/Task.h>

namespace streamr::protorpc {
class RoutedHelloRpcService {
public:
   virtual ~RoutedHelloRpcService() = default;
   virtual RoutedHelloResponse sayHello(const RoutedHelloRequest& request, const ProtoCallContext& callContext) = 0;
}; // class RoutedHelloRpcService
}; // namespace streamr::protorpc

#endif // STREAMR_PROTORPC_ROUTEDHELLORPC_SERVER_PB_H

