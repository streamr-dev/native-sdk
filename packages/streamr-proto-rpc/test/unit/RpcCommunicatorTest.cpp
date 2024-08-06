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

#include "HelloRpc.pb.h"
#include "streamr-proto-rpc/ProtoCallContext.hpp"

namespace streamr::protorpc {

class RpcCommunicatorTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(RpcCommunicatorTest, TestCanRegisterRpcMethod) {
    RpcCommunicator communicator;
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "test",
        [](const HelloRequest& request,
           const ProtoCallContext& /* context */) -> HelloResponse {
            HelloResponse response;
            response.set_greeting("Hello, " + request.myname());
            return response;
        });
    EXPECT_EQ(true, true);
}

TEST_F(RpcCommunicatorTest, TestCanMakeRpcCall) {
    RpcCommunicator communicator1;
    communicator1.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& request,
           const ProtoCallContext& /* context */) -> HelloResponse {
            HelloResponse response;
            SLogger::info(
                "TestCanMakeRpcCall request.myname():", request.myname());
            response.set_greeting("Hello, " + request.myname());
            return response;
        });
    SLogger::info("TestCanMakeRpcCall recisterRpcMethod called");
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
    HelloRequest request;
    request.set_myname("Test");
    SLogger::info("TestCanMakeRpcCall set_myname called");
    auto result = folly::coro::blockingWait(
        communicator2
            .callRemote<HelloResponse, HelloRequest>(
                "testFunction", request, ProtoCallContext())
            .scheduleOn(folly::getGlobalCPUExecutor().get()));
    SLogger::info("TestCanMakeRpcCall callRemote called");
    EXPECT_EQ("Hello, Test", result.greeting());
}

} // namespace streamr::protorpc