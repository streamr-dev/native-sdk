//
//  Hello.cpp
//  StreamrProtoRpcExampleApp
//
//  Created by Santtu Rantanen on 21.8.2024.
//
#define GLOG_USE_GLOG_EXPORT

#include "Hello.hpp"
#include <iostream>
#include <folly/coro/blockingWait.h>
#include "HelloRpc.client.pb.h"
#include "HelloRpc.pb.h"
#include "HelloRpc.server.pb.h"
#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"

using streamr::protorpc::HelloRpcService;
using streamr::protorpc::HelloRpcServiceClient;
using streamr::protorpc::ProtoCallContext;
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::RpcMessage;

class HelloService : public HelloRpcService {
public:
    HelloResponse sayHello(
        const HelloRequest& request,
        const ProtoCallContext& /* callContext */) override {
        HelloResponse response;
        response.set_greeting("Hello, " + request.myname());
        return response;
    }
};

std::string sayHello() {
    RpcCommunicator communicator1;
    HelloService helloService;
    
    communicator1.registerRpcMethod<HelloRequest, HelloResponse>(
            "sayHello",
            std::bind( // NOLINT(modernize-avoid-bind)
                &HelloService::sayHello,
                &helloService,
                std::placeholders::_1,
                std::placeholders::_2));
    
    // Setup client
    RpcCommunicator communicator2;
    HelloRpcServiceClient helloClient(communicator2);
    
    // Simulate a network connection between the client and server
    communicator1.setOutgoingMessageCallback(
            [&communicator2](
                const RpcMessage& message,
                const std::string& /* requestId */,
                const ProtoCallContext& /* context */) -> void {
                    communicator2.handleIncomingMessage(message, ProtoCallContext());
                });
    
    communicator2.setOutgoingMessageCallback(
            [&communicator1](
                const RpcMessage& message,
                const std::string& /* requestId */,
                const ProtoCallContext& /* context */) -> void {
                    communicator1.handleIncomingMessage(message, ProtoCallContext());
                });
    
    // Make the RPC call
    
    HelloRequest request;
    request.set_myname("Alice");
    auto response = folly::coro::blockingWait(
            helloClient.sayHello(request, ProtoCallContext()));
    return response.greeting();
}
