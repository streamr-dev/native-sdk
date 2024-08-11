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
public:
    RpcCommunicatorTest() : executor(10) {} // NOLINT

protected:
    RpcCommunicator communicator1; // NOLINT
    RpcCommunicator communicator2; // NOLINT
    folly::CPUThreadPoolExecutor executor; // NOLINT
    void SetUp() override {}
};

void registerTestRcpMethod(RpcCommunicator& communicator) {
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& request,
           const ProtoCallContext& /* context */) -> HelloResponse {
            HelloResponse response;
            SLogger::info("testFunction() request.myname():", request.myname());
            response.set_greeting("Hello, " + request.myname());
            return response;
        });
}

template <typename T>
void registerThrowingRcpMethod(RpcCommunicator& communicator) {
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& /* request */,
           const ProtoCallContext& /* context */) -> HelloResponse {
            throw T("TestException");
        });
}

void registerThrowingTestRcpMethodUnknown(RpcCommunicator& communicator) {
    const int unknownThrow = 42;
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& /* request */,
           const ProtoCallContext& /* context */) -> HelloResponse {
            throw unknownThrow; // NOLINT
        });
}

void setOutgoingCallback(RpcCommunicator& sender, RpcCommunicator& receiver) {
    sender.setOutgoingMessageCallback(
        [&receiver](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageCallback()");
            receiver.handleIncomingMessage(message, ProtoCallContext());
        });
}

void setOutgoingCallbacks(
    RpcCommunicator& communicator1, RpcCommunicator& communicator2) {
    setOutgoingCallback(communicator2, communicator1);
    setOutgoingCallback(communicator1, communicator2);
}

template <typename T>
void setOutgoingCallbackWithException(RpcCommunicator& sender) {
    sender.setOutgoingMessageCallback(
        [](const RpcMessage& /* message */,
           const std::string& /* requestId */,
           const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageCallback() throws");
            throw T("TestException");
        });
}

void setOutgoingCallbackWithThrownUnknown(RpcCommunicator& sender) {
    const int unknownThrow = 42;
    sender.setOutgoingMessageCallback(
        [](const RpcMessage& /* message */,
           const std::string& /* requestId */,
           const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageCallback() throws");
            throw unknownThrow; // NOLINT
        });
}

auto sendHelloRequest(
    RpcCommunicator& sender, folly::CPUThreadPoolExecutor* executor) {
    HelloRequest request;
    request.set_myname("Test");
    return folly::coro::blockingWait(
        sender
            .callRemote<HelloResponse, HelloRequest>(
                "testFunction", request, ProtoCallContext())
            .scheduleOn(executor));
}

auto sendHelloNotification(
    RpcCommunicator& sender, folly::CPUThreadPoolExecutor* executor) {
    HelloRequest request;
    request.set_myname("Test");
    folly::coro::blockingWait(
        sender
            .notifyRemote<HelloRequest>(
                "testFunction", request, ProtoCallContext())
            .scheduleOn(executor));
}

void verifyClientError(
    const RpcClientError& ex,
    const ErrorCode expectedErrorCode,
    const std::string& expectedOriginalErrorInfo) {
    SLogger::info("Caught RpcClientError", ex.what());
    EXPECT_EQ(ex.code, expectedErrorCode);
    EXPECT_TRUE(ex.originalErrorInfo.has_value());
    EXPECT_EQ(ex.originalErrorInfo.value(), expectedOriginalErrorInfo);
}

void registerSleepingTestRcpMethod(RpcCommunicator& communicator) {
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& request,
           const ProtoCallContext& /* context */) -> HelloResponse {
            SLogger::info("TestSleepingRpcMethod sleeping 5s");
            std::this_thread::sleep_for(std::chrono::seconds(5)); // NOLINT
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
    registerTestRcpMethod(communicator1);
    setOutgoingCallbacks(communicator1, communicator2);
    auto result = sendHelloRequest(communicator2, &executor);
    EXPECT_EQ("Hello, Test", result.greeting());
}

TEST_F(RpcCommunicatorTest, TestCallRemoteClientThrowsRuntimeError) {
    registerTestRcpMethod(communicator1);
    setOutgoingCallbackWithException<std::runtime_error>(communicator2);
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const RpcClientError& ex) {
        verifyClientError(ex, ErrorCode::RPC_CLIENT_ERROR, "TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteClientThrowsFailedToParse) {
    registerTestRcpMethod(communicator1);
    setOutgoingCallbackWithException<FailedToParse>(communicator2);
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const RpcClientError& ex) {
        verifyClientError(
            ex,
            ErrorCode::RPC_CLIENT_ERROR,
            "code: FAILED_TO_PARSE, message: TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteClientThrowsUnknownError) {
    registerTestRcpMethod(communicator1);
    setOutgoingCallbackWithThrownUnknown(communicator2);
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (...) {
        EXPECT_TRUE(true);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteServerThrowsRuntimeError) {
    registerThrowingRcpMethod<std::runtime_error>(communicator1);
    setOutgoingCallbacks(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const RpcServerError& ex) {
        EXPECT_EQ(ex.code, ErrorCode::RPC_SERVER_ERROR);
        EXPECT_TRUE(
            ex.errorClassName.find("runtime_error") != std::string::npos);
        EXPECT_EQ(ex.message, "TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteServerThrowsUnknownRpcMethod) {
    registerThrowingRcpMethod<UnknownRpcMethod>(communicator1);
    setOutgoingCallbacks(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const UnknownRpcMethod& ex) {
        EXPECT_EQ(ex.code, ErrorCode::UNKNOWN_RPC_METHOD);
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteServerThrowsRcpTimeout) {
    registerThrowingRcpMethod<RpcTimeout>(communicator1);
    setOutgoingCallbacks(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const RpcTimeout& ex) {
        EXPECT_EQ(ex.code, ErrorCode::RPC_TIMEOUT);
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteServerThrowsFailedToParse) {
    registerThrowingRcpMethod<FailedToParse>(communicator1);
    setOutgoingCallbacks(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const RpcServerError& ex) {
        EXPECT_EQ(ex.code, ErrorCode::RPC_SERVER_ERROR);
        EXPECT_EQ(ex.errorCode, "FAILED_TO_PARSE");
        EXPECT_TRUE(
            ex.errorClassName.find("FailedToParse") != std::string::npos);
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteServerThrowsUnknown) {
    registerThrowingTestRcpMethodUnknown(communicator1);
    setOutgoingCallbacks(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (...) {
        EXPECT_TRUE(true);
    }
}

TEST_F(RpcCommunicatorTest, TestCanNotifyRemote) {
    std::string requestMsg;
    communicator1.registerRpcNotification<HelloRequest>(
        "testFunction",
        [&requestMsg](
            const HelloRequest& request, const ProtoCallContext& /* context */)
            -> void { requestMsg = request.DebugString(); });
    setOutgoingCallback(communicator2, communicator1);
    sendHelloNotification(communicator2, &executor);
    EXPECT_EQ(requestMsg, "myName: \"Test\"\n");
}

TEST_F(RpcCommunicatorTest, TestNotifyRemoteClientThrowsRuntimeError) {
    setOutgoingCallbackWithException<std::runtime_error>(communicator2);
    try {
        sendHelloNotification(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const RpcClientError& ex) {
        verifyClientError(ex, ErrorCode::RPC_CLIENT_ERROR, "TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestNotifyRemoteClientThrowsFailedToParse) {
    setOutgoingCallbackWithException<FailedToParse>(communicator2);
    try {
        sendHelloNotification(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const RpcClientError& ex) {
        verifyClientError(
            ex,
            ErrorCode::RPC_CLIENT_ERROR,
            "code: FAILED_TO_PARSE, message: TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestRpcTimeoutOnClientSide) {
    RpcCommunicator communicator1;
    registerTestRcpMethod(communicator1);

    RpcCommunicator communicator2;
    communicator2.setOutgoingMessageCallback(
        [&communicator1](
            const RpcMessage& /* message */,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("setOutgoingMessageCallback() sleeping 5s");
            std::this_thread::sleep_for(std::chrono::seconds(5)); // NOLINT
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

    communicator2.setOutgoingMessageCallback(
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
    communicator1.setOutgoingMessageCallback(
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

TEST_F(RpcCommunicatorTest, TestRpcTimeoutOnClientSideForNotification) {
    RpcCommunicator communicator1;
    RpcCommunicator communicator2;
    std::string requestMsg;

    communicator1.registerRpcNotification<HelloRequest>(
        "testFunction",
        [&requestMsg](
            const HelloRequest& request, const ProtoCallContext& /* context */)
            -> void { requestMsg = request.DebugString(); });

    HelloRequest request;
    request.set_myname("Test");

    communicator2.setOutgoingMessageCallback(
        [&communicator1](
            const RpcMessage& /* message */,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("setOutgoingMessageCallback() sleeping 5s");
            std::this_thread::sleep_for(std::chrono::seconds(5)); // NOLINT
        });
    try {
        folly::coro::blockingWait(
            communicator2
                .notifyRemote<HelloRequest>(
                    "testFunction",
                    request,
                    ProtoCallContext{.timeout = 50}) // NOLINT
                .scheduleOn(folly::getGlobalCPUExecutor().get()));
        // Test fails here
        EXPECT_TRUE(false);
    } catch (const RpcTimeout& ex) {
        SLogger::info(
            "TestRpcTimeoutOnClientSideForNotification caught RpcTimeout",
            ex.what());
    } catch (const std::exception& ex) {
        SLogger::info(
            "TestRpcTimeoutOnClientSideForNotification caught unknown exception",
            ex.what());
        EXPECT_TRUE(false);
    }
}

} // namespace streamr::protorpc
