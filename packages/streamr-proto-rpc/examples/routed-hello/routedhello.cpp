#include <iostream>
#include <folly/coro/BlockingWait.h>
#include "RoutedHelloRpc.client.pb.h"
#include "RoutedHelloRpc.pb.h"
#include "RoutedHelloRpc.server.pb.h"
#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"

using streamr::protorpc::RoutedHelloRpcService;
using streamr::protorpc::RoutedHelloRpcServiceClient;
using streamr::protorpc::ProtoCallContext;
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::RpcMessage;

class RoutedHelloService : public RoutedHelloRpcService {
public:
    std::string mServiceId;

    RoutedHelloService(const std::string& serviceId) : mServiceId(serviceId) {}

    RoutedHelloResponse sayHello(
        const RoutedHelloRequest& request,
        const ProtoCallContext& callContext) override {
        RoutedHelloResponse response;
        std::string sourceId = callContext.at("sourceId");   
        response.set_greeting("Hello, " + request.myname());
        return response;
    }
};

int main() {
    std::unordered_map<std::string, RpcCommunicator> clientCommunicators;
    clientCommunicators.emplace("1", RpcCommunicator());
   // auto& communicator1 = clientCommunicators.at("1");
   // clientCommunicators["1"] = 
    // Setup server 1
  //  RpcCommunicator communicator1; 
    RoutedHelloService helloService1("1");
    communicator1.registerRpcMethod<RoutedHelloRequest, RoutedHelloResponse>(
        "sayHello",
        std::bind( // NOLINT(modernize-avoid-bind)
            &RoutedHelloService::sayHello,
            &helloService1,
            std::placeholders::_1,
            std::placeholders::_2));

    // Setup server 2
    RpcCommunicator communicator2; 
    RoutedHelloService helloService2("2");
    communicator2.registerRpcMethod<RoutedHelloRequest, RoutedHelloResponse>(
        "sayHello",
        std::bind( // NOLINT(modernize-avoid-bind)
            &RoutedHelloService::sayHello,
            &helloService2,
            std::placeholders::_1,
            std::placeholders::_2));

    // Setup client 1
    RoutedHelloRpcServiceClient helloClient1(communicator1);
  //  clientCommunicators["Client1"] = std::move(communicator1)
  //  clientCommunicators["1"] = communicator1;

/*

    // Setup server
    RpcCommunicator communicator1;
    HelloService helloService;

    communicator1.registerRpcMethod<HelloRequest, HelloResponse>(
        "sayHello",
        std::bind( // NOLINT(modernize-avoid-bind)
            &HelloService::sayHello,
            &helloService,
            std::placeholders::_1,
            std::placeholders::_2));  
   */
    return 0;
}
