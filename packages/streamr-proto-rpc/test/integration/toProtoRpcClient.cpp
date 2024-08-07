#include <gtest/gtest.h>
#include <streamr-proto-rpc/RpcCommunicator.hpp>
#include <folly/Portability.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/ManualExecutor.h>
#include <folly/experimental/coro/Baton.h>
#include <folly/experimental/coro/BlockingWait.h>
#include <folly/experimental/coro/Collect.h>
#include <folly/experimental/coro/CurrentExecutor.h>
#include <folly/experimental/coro/Generator.h>
#include <folly/experimental/coro/GtestHelpers.h>
#include <folly/experimental/coro/Mutex.h>
#include <folly/experimental/coro/Sleep.h>
#include <folly/experimental/coro/Task.h>
#include <folly/io/async/Request.h>
#include "HelloRpc.client.pb.h"
#include "HelloRpc.pb.h"
#include "streamr-proto-rpc/ProtoCallContext.hpp"

// NOLINTBEGIN

namespace streamr::protorpc {

class ProtoRpcClientTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

void registerTestRcpMethod(RpcCommunicator& communicator) {
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "sayHello",
        [](const HelloRequest& request,
           const ProtoCallContext& /* context */) -> HelloResponse {
            HelloResponse response;
            SLogger::info(
                "TestCanMakeRpcCall request.myname():", request.myname());
            response.set_greeting("Hello, " + request.myname());
            return response;
        });
}

TEST_F(ProtoRpcClientTest, TestCanMakeRpcCall) {
    RpcCommunicator communicator1;
    registerTestRcpMethod(communicator1);
    RpcCommunicator communicator2;
    communicator2.setOutgoingMessageListener(
        [&communicator1](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageListener()");
            communicator1.handleIncomingMessage(message, ProtoCallContext());
        });
    communicator1.setOutgoingMessageListener(
        [&communicator2](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageListener()");
            communicator2.handleIncomingMessage(message, ProtoCallContext());
        });

    SLogger::info("TestCanMakeRpcCall setOutgoingMessageListener called");

    HelloRpcServiceClient client(communicator2);
    HelloRequest request;
    request.set_myname("Test");
    auto result =
        folly::coro::blockingWait(client.sayHello(request, ProtoCallContext()));
    SLogger::info("TestCanMakeRpcCall callRemote called");
    EXPECT_EQ("Hello, Test", result.greeting());
}

TEST_F(ProtoRpcClientTest, TestCanCallRemoteWhichThrows) {
    RpcCommunicator communicator1;
    registerTestRcpMethod(communicator1);
    SLogger::info("TestCanCallRemoteWhichThrows registerRpcMethod called");
    RpcCommunicator communicator2;
    communicator2.setOutgoingMessageListener(
        [&communicator1](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("setOutgoingMessageListener() Before Exception:");
            throw std::runtime_error("TestException");
        });
    SLogger::info(
        "TestCanCallRemoteWhichThrows setOutgoingMessageListener called");
    HelloRpcServiceClient client(communicator2);
    HelloRequest request;
    request.set_myname("Test");
    EXPECT_THROW(
        folly::coro::blockingWait(client.sayHello(request, ProtoCallContext())),
        std::exception);
}

} // namespace streamr::protorpc

// NOLINTEND