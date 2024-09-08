#include <iostream>
#include <string_view>
#include <map>
#include <folly/coro/BlockingWait.h>
#include "RoutedHelloRpc.client.pb.h"
#include "RoutedHelloRpc.pb.h"
#include "RoutedHelloRpc.server.pb.h"
#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"

using streamr::protorpc::ProtoCallContext;
using ::RoutedHelloResponse;
using streamr::protorpc::RoutedHelloRpcService;
using streamr::protorpc::RoutedHelloRpcServiceClient;
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::RpcMessage;
using RpcCommunicatorType = streamr::protorpc::RpcCommunicator<ProtoCallContext>;

class RoutedHelloService : public RoutedHelloRpcService<ProtoCallContext> {
private:
    std::string mServiceId;

public:
    explicit RoutedHelloService(std::string_view serviceId)
        : mServiceId(std::string(serviceId)) {}

    RoutedHelloResponse sayHello(
        const RoutedHelloRequest& request,
        const ProtoCallContext& callContext) override {
        RoutedHelloResponse response;
        std::string sourceId = callContext.at("sourceId");
        std::cout << "sayHello() called on server " << mServiceId
                  << " with context parameter sourceId " << sourceId << "\n";
        response.set_greeting("Hello, " + request.myname() + "!");
        return response;
    }
    [[nodiscard]] std::string getServiceId() const { return mServiceId; }
};

int main() {
    std::map<std::string, std::shared_ptr<RpcCommunicatorType>>
        clientCommunicators;

    // Setup server
    
    RpcCommunicatorType serverCommunicator1;
    RoutedHelloService helloService1("1");

    serverCommunicator1
        .registerRpcMethod<RoutedHelloRequest, RoutedHelloResponse>(
            "sayHello",
            std::bind( // NOLINT(modernize-avoid-bind)
                &RoutedHelloService::sayHello,
                &helloService1,
                std::placeholders::_1,
                std::placeholders::_2));

    // Setup server 2
    
    RpcCommunicatorType serverCommunicator2;
    RoutedHelloService helloService2("2");

    serverCommunicator2
        .registerRpcMethod<RoutedHelloRequest, RoutedHelloResponse>(
            "sayHello",
            std::bind( // NOLINT(modernize-avoid-bind)
                &RoutedHelloService::sayHello,
                &helloService2,
                std::placeholders::_1,
                std::placeholders::_2));

    // Setup client1
    
    auto communicator1 = std::make_shared<RpcCommunicatorType>();
    RoutedHelloRpcServiceClient helloClient1(*communicator1);
    clientCommunicators["1"] = communicator1;

    // Setup client2
    
    auto communicator2 = std::make_shared<RpcCommunicatorType>();
    RoutedHelloRpcServiceClient helloClient2(*communicator2);
    clientCommunicators["2"] = communicator2;

    // Simulate a network connections
    
    serverCommunicator1.setOutgoingMessageCallback(
        [&clientCommunicators](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& context) -> void {
            // Send the reply message to the calling client based on
            // sourceId passed
            // through the network stack in the context information
            const std::string sourceId = context.at("sourceId");
            clientCommunicators[sourceId]->handleIncomingMessage(
                message, context);
        });

    serverCommunicator2.setOutgoingMessageCallback(
        [&clientCommunicators](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& context) -> void {
            const std::string sourceId = context.at("sourceId");
            clientCommunicators[sourceId]->handleIncomingMessage(
                message, context);
        });

    communicator1->setOutgoingMessageCallback(
        [&clientCommunicators, &serverCommunicator1, &serverCommunicator2](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& context) -> void {
            // Choose the server to send the message to based on context
            // information passed through the RPC stack as client context
            // information
            RpcCommunicatorType* server;
            std::string targetServerId = context.at("targetServerId");

            if (targetServerId == "2") {
                server = &serverCommunicator2;
            } else {
                server = &serverCommunicator1;
            }

            // Here you would transport the msgBody over network to the server
            // ...
            // At the server you would pass the id of the calling client as
            // context information to the server. The server is free to choose
            // what to use as the id of the calling client; it might use, for
            // example, the id of the network socket, something negotiated
            // during connection handshake, or something passed on in every
            // network payload.
            //
            // The context information gets passed uncahged through the RPC
            // stack, so the reply message can be routed to the correct client.

            ProtoCallContext serverContext;
            serverContext["sourceId"] = "1";

            server->handleIncomingMessage(message, serverContext);
        });

    communicator2->setOutgoingMessageCallback(
        [&clientCommunicators, &serverCommunicator1, &serverCommunicator2](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& context) -> void {
            RpcCommunicatorType* server;
            std::string targetServerId = context.at("targetServerId");

            if (targetServerId == "2") {
                server = &serverCommunicator2;
            } else {
                server = &serverCommunicator1;
            }

            ProtoCallContext serverContext;
            serverContext["sourceId"] = "2";

            server->handleIncomingMessage(message, serverContext);
        });

    // Make some RPC calls

    RoutedHelloRequest request;
    request.set_myname("Alice");
    ProtoCallContext context;
    context["targetServerId"] = "2";

    auto response =
        folly::coro::blockingWait(helloClient1.sayHello(std::move(request), std::move(context)));

    std::cout << "Client 1 (Alice) got message from server: " +
            response.greeting()
              << "\n";

    RoutedHelloRequest request2;
    request2.set_myname("Bob");
    ProtoCallContext context2;
    context2["targetServerId"] = "1";

    auto response2 =
        folly::coro::blockingWait(helloClient2.sayHello(std::move(request2), std::move(context2)));

    std::cout << "Client 2 (Bob) got message from server: " +
            response2.greeting()
              << "\n";

    return 0;
}
