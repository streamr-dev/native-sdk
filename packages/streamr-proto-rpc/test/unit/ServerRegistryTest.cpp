#include <gtest/gtest.h>

#include <google/protobuf/any.pb.h>
#include "HelloRpc.pb.h"
#include "streamr-logger/SLogger.hpp"
#include "streamr-proto-rpc/ServerRegistry.hpp"

// NOLINTBEGIN

namespace streamr::protorpc {

class ServerRegistryTest : public ::testing::Test {
protected:
    ServerRegistry registry;
    void SetUp() override {}
};

RpcMessage createHelloRcpMessage(std::optional<std::string_view> method = "sayHello") {
    HelloRequest request;
    request.set_myname("testUser");
    RpcMessage requestWrapper;
    auto& header = *requestWrapper.mutable_header();
    if (method.has_value()) {
        header["method"] = method.value();
    }
    header["request"] = "request";
    requestWrapper.mutable_body()->PackFrom(request);
    requestWrapper.set_requestid("request-id");
    return requestWrapper;
}

TEST_F(ServerRegistryTest, TestCanHandleRequest) {
    RpcMessage requestWrapper = createHelloRcpMessage();
    registry.registerRpcMethod<HelloRequest, HelloResponse>(
        "sayHello",
        +[](const HelloRequest& request,
            const ProtoCallContext& /* callContext */) -> HelloResponse {
            HelloResponse response;
            response.set_greeting("hello " + request.myname());

            return response;
        });
    auto res = registry.handleRequest(requestWrapper, {});
    HelloResponse helloResponse;
    auto unpacked = res.UnpackTo(&helloResponse);
    EXPECT_EQ(helloResponse.greeting(), "hello testUser");
}

TEST_F(ServerRegistryTest, HandleUnknownRpcMethod) {
    RpcMessage requestWrapper = createHelloRcpMessage("unknownMethod");
    EXPECT_THROW(registry.handleRequest(requestWrapper, {}), UnknownRpcMethod);
}

TEST_F(ServerRegistryTest, HandleRequestWithMissingMethodHeader) {
    RpcMessage requestWrapper = createHelloRcpMessage(std::nullopt);
    EXPECT_THROW(registry.handleRequest(requestWrapper, {}), UnknownRpcMethod);
}

TEST_F(ServerRegistryTest, RegisterAndHandleRpcNotification) {
    RpcMessage requestWrapper = createHelloRcpMessage("testNotification");
    bool notificationCalled = false;
    auto rpcNotification = [&notificationCalled](
                               const HelloRequest& /* request */,
                               const ProtoCallContext& /* context */) -> void {
        notificationCalled = true;
    };
    registry.registerRpcNotification<HelloRequest>(
        "testNotification", rpcNotification);
    registry.handleNotification(requestWrapper, {});
    EXPECT_TRUE(notificationCalled);
}

TEST_F(ServerRegistryTest, HandleUnknownRpcNotification) {
    RpcMessage requestWrapper = createHelloRcpMessage("unknownNotification");
    EXPECT_THROW(
        registry.handleNotification(requestWrapper, {}), UnknownRpcMethod);
}

TEST_F(ServerRegistryTest, HandleNotificationWithMissingMethodHeader) {
    RpcMessage requestWrapper = createHelloRcpMessage(std::nullopt);
    EXPECT_THROW(
        registry.handleNotification(requestWrapper, {}), UnknownRpcMethod);
}

} // namespace streamr::protorpc

// NOLINTEND