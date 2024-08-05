#include <gtest/gtest.h>

#include <google/protobuf/any.pb.h>
#include "HelloRpc.pb.h"
#include "streamr-logger/SLogger.hpp"
#include "streamr-proto-rpc/ServerRegistry.hpp"

namespace streamr::protorpc {

// using google::protobuf::Any;

class ServerRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

/*
 const request: HelloRequest = {
        myName: 'test'
    }

    const requestWrapper: RpcMessage = {
        header: {
            method: 'sayHello',
            request: 'request'
        },
        body: Any.pack(request, HelloRequest),
        requestId: 'request-id'
    }
*/

TEST_F(ServerRegistryTest, TestCanBindMethods) {
    ServerRegistry registry;
    HelloRequest request;
    request.set_myname("testUser");
    RpcMessage requestWrapper;
    auto& header = *requestWrapper.mutable_header();
    header["method"] = "sayHello";
    header["request"] = "request";
    requestWrapper.mutable_body()->PackFrom(request);
    requestWrapper.set_requestid("request-id");
    registry.registerRpcMethod<HelloRequest, HelloResponse>(
        "sayHello",
        +[](const HelloRequest& request,
            const ProtoCallContext& /* callContext */) -> HelloResponse {
            HelloResponse response;
            response.set_greeting("hello " + request.myname());

            return response;
        });
    SLogger::info("requestWrapper", requestWrapper.header().size());
    auto res = registry.handleRequest(requestWrapper, {});
    HelloResponse helloResponse;
    auto unpacked = res.UnpackTo(&helloResponse);
    EXPECT_EQ(helloResponse.greeting(), "hello testUser");
}

TEST_F(ServerRegistryTest, HandleUnknownRpcMethod) {
    // Create a test RPC message with an unknown method
    ServerRegistry registry;
    HelloRequest request;
    request.set_myname("testUser");
    RpcMessage requestWrapper;
    auto& header = *requestWrapper.mutable_header();
    header["method"] = "unknownMethod";
    header["request"] = "request";
    requestWrapper.mutable_body()->PackFrom(request);
    requestWrapper.set_requestid("request-id");
    EXPECT_THROW(registry.handleRequest(requestWrapper, {}), UnknownRpcMethod);
}

TEST_F(ServerRegistryTest, HandleRequestWithMissingMethodHeader) {
    // Create a test RPC message with an unknown method
    ServerRegistry registry;
    HelloRequest request;
    request.set_myname("testUser");
    RpcMessage requestWrapper;
    auto& header = *requestWrapper.mutable_header();
    header["request"] = "request";
    requestWrapper.mutable_body()->PackFrom(request);
    requestWrapper.set_requestid("request-id");
    EXPECT_THROW(registry.handleRequest(requestWrapper, {}), UnknownRpcMethod);
}

TEST_F(ServerRegistryTest, RegisterAndHandleRpcNotification) {
    ServerRegistry registry;
    HelloRequest request;
    request.set_myname("testUser");
    RpcMessage requestWrapper;
    auto& header = *requestWrapper.mutable_header();
    header["method"] = "testNotification";
    header["request"] = "request";
    requestWrapper.mutable_body()->PackFrom(request);
    requestWrapper.set_requestid("request-id");
    bool notificationCalled = false;
    auto rpcNotification = [&notificationCalled](
                               const HelloRequest& /* request */,
                               const ProtoCallContext& /* context */) -> void {
        notificationCalled = true;
    };
    // Register the RPC notification
    registry.registerRpcNotification<HelloRequest>(
        "testNotification", rpcNotification);
    registry.handleNotification(requestWrapper, {});
    // Verify the notification was called
    EXPECT_TRUE(notificationCalled);
}

TEST_F(ServerRegistryTest, HandleUnknownRpcNotification) {
    // Create a test RPC message with an unknown notification
    ServerRegistry registry;
    HelloRequest request;
    request.set_myname("testUser");
    RpcMessage requestWrapper;
    auto& header = *requestWrapper.mutable_header();
    header["method"] = "unknownNotification";
    header["request"] = "request";
    requestWrapper.mutable_body()->PackFrom(request);
    requestWrapper.set_requestid("request-id");
    // Expect an exception to be thrown
    EXPECT_THROW(
        registry.handleNotification(requestWrapper, {}), UnknownRpcMethod);
}

TEST_F(ServerRegistryTest, HandleNotificationWithMissingMethodHeader) {
    // Create a test RPC message without a method header
    ServerRegistry registry;
    HelloRequest request;
    request.set_myname("testUser");
    RpcMessage requestWrapper;
    auto& header = *requestWrapper.mutable_header();
    header["request"] = "request";
    requestWrapper.mutable_body()->PackFrom(request);
    requestWrapper.set_requestid("request-id");
    // Expect an exception to be thrown
    EXPECT_THROW(
        registry.handleNotification(requestWrapper, {}), UnknownRpcMethod);
}

} // namespace streamr::protorpc