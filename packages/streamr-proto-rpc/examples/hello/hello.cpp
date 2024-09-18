#include <exception>
#include <iostream>
#include <folly/coro/BlockingWait.h>
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
using RpcCommunicatorType = streamr::protorpc::RpcCommunicator<ProtoCallContext>;

class HelloService : public HelloRpcService<ProtoCallContext> {
public:
    HelloResponse sayHello(
        const HelloRequest& request,
        const ProtoCallContext& /* callContext */) override {
        HelloResponse response;
        response.set_greeting("Hello, " + request.myname());
        return response;
    }
};

int main() {
    // Setup server
    RpcCommunicatorType communicator1;
    HelloService helloService;

    communicator1.registerRpcMethod<HelloRequest, HelloResponse>(
        "sayHello",
        std::bind( // NOLINT(modernize-avoid-bind)
            &HelloService::sayHello,
            &helloService,
            std::placeholders::_1,
            std::placeholders::_2));

    // Setup client
    RpcCommunicatorType communicator2;
    HelloRpcServiceClient helloClient(communicator2);

    // Simulate a network connection between the client and server
    communicator1.setOutgoingMessageCallback(
        [&communicator2](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const std::optional<std::function<void(std::exception_ptr)>>& /* errorCallback */,
            const ProtoCallContext& /* context */) -> void {
            communicator2.handleIncomingMessage(message, ProtoCallContext());
        });

    communicator2.setOutgoingMessageCallback(
        [&communicator1](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const std::optional<std::function<void(std::exception_ptr)>>& /* errorCallback */,
            const ProtoCallContext& /* context */) -> void {
            communicator1.handleIncomingMessage(message, ProtoCallContext());
        });

    // Make the RPC call

    HelloRequest request;
    request.set_myname("Alice");
    auto response = folly::coro::blockingWait(
        helloClient.sayHello(std::move(request), ProtoCallContext()));
    std::cout << response.greeting() << "\n";

    return 0;
}
