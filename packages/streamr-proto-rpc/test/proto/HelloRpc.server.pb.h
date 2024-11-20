// Generated by the protocol buffer streamr pluging. DO NOT EDIT!
// Generated from protobuf file "HelloRpc.proto"

#ifndef STREAMR_PROTORPC_HELLORPC_SERVER_PB_H
#define STREAMR_PROTORPC_HELLORPC_SERVER_PB_H

#include "HelloRpc.pb.h" // NOLINT
#include <folly/experimental/coro/Task.h>

namespace streamr::protorpc {
template <typename CallContextType>
class HelloRpcService {
public:
   virtual ~HelloRpcService() = default;
   virtual HelloResponse sayHello(const HelloRequest& request, const CallContextType& callContext) = 0;
}; // class HelloRpcService
}; // namespace streamr::protorpc

#endif // STREAMR_PROTORPC_HELLORPC_SERVER_PB_H

