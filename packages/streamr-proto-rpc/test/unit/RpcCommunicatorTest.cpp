#include <exception>
#include <thread>
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
#include "streamr-proto-rpc/Errors.hpp"
#include "streamr-proto-rpc/ProtoCallContext.hpp"

namespace streamr::protorpc {

class RpcCommunicatorTest : public ::testing::Test {
protected:
    // RpcCommunicator communicator; // NOLINT

    void SetUp() override {}
};

void registerTestRcpMethod(RpcCommunicator& communicator) {
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& request,
           const ProtoCallContext& /* context */) -> HelloResponse {
            HelloResponse response;
            SLogger::info(
                "TestCanMakeRpcCall request.myname():", request.myname());
            response.set_greeting("Hello, " + request.myname());
            return response;
        });
}

void registerThrowingTestRcpMethod(RpcCommunicator& communicator) {
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& /* request */,
           const ProtoCallContext& /* context */) -> HelloResponse {
            throw std::runtime_error("TestServerException");
            // HelloResponse response;
            //  return response;
        });
}

void registerSleepingTestRcpMethod(RpcCommunicator& communicator) {
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& request,
           const ProtoCallContext& /* context */) -> HelloResponse {
            SLogger::info("TestSleepingRpcMethod sleeping 1s");
            std::this_thread::sleep_for(std::chrono::seconds(1)); // NOLINT
            HelloResponse response;
            response.set_greeting("Hello, " + request.myname());
            return response;
        });
}

TEST_F(RpcCommunicatorTest, TestCanRegisterRpcMethod) {
    RpcCommunicator communicator;
    registerTestRcpMethod(communicator);
    EXPECT_EQ(true, true);
}

TEST_F(RpcCommunicatorTest, TestCanMakeRpcCall) {
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

TEST_F(RpcCommunicatorTest, TestCanCallRemoteClientThrows) {
    RpcCommunicator communicator1;
    registerTestRcpMethod(communicator1);
    SLogger::info("TestCanCallRemoteWhichThrows registerRpcMethod called");
    RpcCommunicator communicator2;
    communicator2.setOutgoingMessageListener(
        [&communicator1](
            const RpcMessage& /* message */,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("setOutgoingMessageListener() Before Exception:");
            throw std::runtime_error("TestException");
        });

    SLogger::info(
        "TestCanCallRemoteWhichThrows setOutgoingMessageListener called");
    HelloRequest request;
    request.set_myname("Test");
    SLogger::info("TestCanCallRemoteWhichThrows set_myname called");
    try {
        folly::coro::blockingWait(
            communicator2
                .callRemote<HelloResponse, HelloRequest>(
                    "testFunction", request, ProtoCallContext())
                .scheduleOn(folly::getGlobalCPUExecutor().get()));
        // Test fails here
        EXPECT_TRUE(false);
    } catch (const RpcClientError& ex) {
        SLogger::info(
            "TestCanCallRemoteWhichThrows caught RpcClientError", ex.what());
        EXPECT_EQ(ex.code, ErrorCode::RPC_CLIENT_ERROR);
        EXPECT_TRUE(ex.originalErrorInfo.has_value());
        EXPECT_EQ(ex.originalErrorInfo.value(), "TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCanCallRemoteServerThrows) {
    RpcCommunicator communicator1;
    registerThrowingTestRcpMethod(communicator1);
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

    HelloRequest request;
    request.set_myname("Test");
    try {
        auto result = folly::coro::blockingWait(
            communicator2
                .callRemote<HelloResponse, HelloRequest>(
                    "testFunction", request, ProtoCallContext())
                .scheduleOn(folly::getGlobalCPUExecutor().get()));
        EXPECT_TRUE(false);
    } catch (const RpcServerError& ex) {
        SLogger::info(
            "TestCanCallRemoteServerThrows caught RpcServerError", ex.what());
        EXPECT_EQ(ex.code, ErrorCode::RPC_SERVER_ERROR);
        EXPECT_TRUE(
            ex.errorClassName.find("runtime_error") != std::string::npos);
        EXPECT_EQ(ex.errorCode, "RPC_SERVER_ERROR");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCanNotifyRemote) {
    RpcCommunicator communicator1;
    std::string requestMsg;
    communicator1.registerRpcNotification<HelloRequest>(
        "testFunction",
        [&requestMsg](
            const HelloRequest& request,
            const ProtoCallContext& /* context */) -> void {
            requestMsg = request.DebugString();
            SLogger::info(
                "TestCanNotifyRemote request:", request.DebugString());
        });

    SLogger::info("TestCanNotifyRemote registerRpcNotification called");
    RpcCommunicator communicator2;
    communicator2.setOutgoingMessageListener(
        [&communicator1](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageListener()");
            communicator1.handleIncomingMessage(message, ProtoCallContext());
        });

    HelloRequest request;
    request.set_myname("Test");
    SLogger::info("TestCanNotifyRemote set_myname called");
    folly::coro::blockingWait(
        communicator2
            .notifyRemote<HelloRequest>(
                "testFunction", request, ProtoCallContext())
            .scheduleOn(folly::getGlobalCPUExecutor().get()));
    SLogger::info("TestCanNotifyRemote callRemote called");
    EXPECT_EQ(requestMsg, "myName: \"Test\"\n");
}

TEST_F(RpcCommunicatorTest, TestCanNotifyRemoteWhichThrows) {
    RpcCommunicator communicator1;
    std::string requestMsg;
    communicator1.registerRpcNotification<HelloRequest>(
        "testFunction",
        [&requestMsg](
            const HelloRequest& request,
            const ProtoCallContext& /* context */) -> void {
            requestMsg = request.DebugString();
            SLogger::info(
                "TestCanNotifyRemoteWhichThrows request:",
                request.DebugString());
        });

    SLogger::info(
        "TestCanNotifyRemoteWhichThrows registerRpcNotification called");
    RpcCommunicator communicator2;
    communicator2.setOutgoingMessageListener(
        [&communicator1](
            const RpcMessage& /* message */,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageListener()");
            throw std::runtime_error("TestException");
        });

    HelloRequest request;
    request.set_myname("Test");
    SLogger::info("TestCanNotifyRemoteWhichThrows set_myname called");
    EXPECT_THROW(
        folly::coro::blockingWait(
            communicator2
                .notifyRemote<HelloRequest>(
                    "testFunction", request, ProtoCallContext())
                .scheduleOn(folly::getGlobalCPUExecutor().get())),
        std::exception);
}

TEST_F(RpcCommunicatorTest, TestRpcTimeoutOnClientSide) {
    RpcCommunicator communicator1;
    registerTestRcpMethod(communicator1);

    RpcCommunicator communicator2;
    communicator2.setOutgoingMessageListener(
        [&communicator1](
            const RpcMessage& /* message */,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("setOutgoingMessageListener() sleeping 5s");
            std::this_thread::sleep_for(std::chrono::seconds(1)); // NOLINT
        });

    HelloRequest request;
    request.set_myname("Test");

    try {
        folly::coro::blockingWait(
            communicator2
                .callRemote<HelloResponse, HelloRequest>(
                    "testFunction",
                    request,
                    ProtoCallContext{.timeout = 50}) // NOLINT
                .scheduleOn(folly::getGlobalCPUExecutor().get()));
        // Test fails here
        EXPECT_TRUE(false);
    } catch (const RpcTimeout& ex) {
        SLogger::info("TestRpcTimeout caught RpcTimeout", ex.what());
    } catch (const std::exception& ex) {
        SLogger::info(
            "TestRpcTimeoutOnClientSide caught unknown exception", ex.what());
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestRpcTimeoutOnServerSide) {
    RpcCommunicator communicator1;
    registerSleepingTestRcpMethod(communicator1);
    RpcCommunicator communicator2;
    std::shared_ptr<std::thread> thread;

    communicator2.setOutgoingMessageListener(
        [&communicator1, &thread](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& context) -> void {
            thread = std::make_shared<std::thread>(
                [&communicator1, message, context]() {
                    SLogger::info("Starting thread for server");
                    communicator1.handleIncomingMessage(message, context);
                });
        });
    communicator1.setOutgoingMessageListener(
        [&communicator2](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& context) -> void {
            communicator2.handleIncomingMessage(message, context);
        });

    HelloRequest request;
    request.set_myname("Test");
    try {
        auto result = folly::coro::blockingWait(
            communicator2
                .callRemote<HelloResponse, HelloRequest>(
                    "testFunction",
                    request,
                    ProtoCallContext{.timeout = 50}) // NOLINT
                .scheduleOn(folly::getGlobalCPUExecutor().get()));
        EXPECT_EQ(true, false);
    } catch (const RpcTimeout& ex) {
        SLogger::info(
            "TestRpcTimeoutOnServerSide caught RpcTimeout", ex.what());
    } catch (...) {
        SLogger::info("TestRpcTimeoutOnServerSide caught unknown exception");
        EXPECT_EQ(true, false);
    }
    thread->join();
}

} // namespace streamr::protorpc
