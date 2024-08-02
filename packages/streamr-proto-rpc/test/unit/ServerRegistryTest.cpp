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
} // namespace streamr::protorpc