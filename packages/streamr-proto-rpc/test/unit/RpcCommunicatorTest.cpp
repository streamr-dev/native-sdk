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
    RpcCommunicator communicator1; // NOLINT
    RpcCommunicator communicator2; // NOLINT
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

void setOutgoingListener(RpcCommunicator& sender, RpcCommunicator& receiver) {
    sender.setOutgoingMessageListener(
        [&receiver](
            const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageListener()");
            receiver.handleIncomingMessage(message, ProtoCallContext());
        });
}

void setOutgoingListeners(
    RpcCommunicator& communicator1, RpcCommunicator& communicator2) {
    setOutgoingListener(communicator2, communicator1);
    setOutgoingListener(communicator1, communicator2);
}

template <typename T>
void setOutgoingListenerWithException(RpcCommunicator& sender) {
    sender.setOutgoingMessageListener(
        [](const RpcMessage& /* message */,
           const std::string& /* requestId */,
           const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageListener() throws");
            throw T("TestException");
        });
}

void setOutgoingListenerWithThrownUnknown(RpcCommunicator& sender) {
    const int unknownThrow = 42;
    sender.setOutgoingMessageListener(
        [](const RpcMessage& /* message */,
           const std::string& /* requestId */,
           const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageListener() throws");
            throw unknownThrow; // NOLINT
        });
}

auto sendHelloRequest(RpcCommunicator& sender) {
    HelloRequest request;
    request.set_myname("Test");
    return folly::coro::blockingWait(
        sender
            .callRemote<HelloResponse, HelloRequest>(
                "testFunction", request, ProtoCallContext())
            .scheduleOn(folly::getGlobalCPUExecutor().get()));
}

auto sendHelloNotification(RpcCommunicator& sender) {
    HelloRequest request;
    request.set_myname("Test");
    folly::coro::blockingWait(
        sender
            .notifyRemote<HelloRequest>(
                "testFunction", request, ProtoCallContext())
            .scheduleOn(folly::getGlobalCPUExecutor().get()));
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

TEST_F(RpcCommunicatorTest, TestCanRegisterRpcMethod) {
    RpcCommunicator communicator;
    registerTestRcpMethod(communicator);
    EXPECT_EQ(true, true);
}

TEST_F(RpcCommunicatorTest, TestCanMakeRpcCall) {
    registerTestRcpMethod(communicator1);
    setOutgoingListeners(communicator1, communicator2);
    auto result = sendHelloRequest(communicator2);
    EXPECT_EQ("Hello, Test", result.greeting());
}

TEST_F(RpcCommunicatorTest, TestCallRemoteClientThrowsRuntimeError) {
    registerTestRcpMethod(communicator1);
    setOutgoingListenerWithException<std::runtime_error>(communicator2);
    try {
        sendHelloRequest(communicator2);
        EXPECT_TRUE(false);
    } catch (const RpcClientError& ex) {
        verifyClientError(ex, ErrorCode::RPC_CLIENT_ERROR, "TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteClientThrowsFailedToParse) {
    registerTestRcpMethod(communicator1);
    setOutgoingListenerWithException<FailedToParse>(communicator2);
    try {
        sendHelloRequest(communicator2);
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
    setOutgoingListenerWithThrownUnknown(communicator2);
    try {
        sendHelloRequest(communicator2);
        EXPECT_TRUE(false);
    } catch (...) {
        EXPECT_TRUE(true);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteServerThrowsRuntimeError) {
    registerThrowingRcpMethod<std::runtime_error>(communicator1);
    setOutgoingListeners(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2);
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
    setOutgoingListeners(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2);
        EXPECT_TRUE(false);
    } catch (const UnknownRpcMethod& ex) {
        EXPECT_EQ(ex.code, ErrorCode::UNKNOWN_RPC_METHOD);
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteServerThrowsRcpTimeout) {
    registerThrowingRcpMethod<RpcTimeout>(communicator1);
    setOutgoingListeners(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2);
        EXPECT_TRUE(false);
    } catch (const RpcTimeout& ex) {
        EXPECT_EQ(ex.code, ErrorCode::RPC_TIMEOUT);
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestCallRemoteServerThrowsFailedToParse) {
    registerThrowingRcpMethod<FailedToParse>(communicator1);
    setOutgoingListeners(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2);
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
    setOutgoingListeners(communicator1, communicator2);
    try {
        sendHelloRequest(communicator2);
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
    setOutgoingListener(communicator2, communicator1);
    sendHelloNotification(communicator2);
    EXPECT_EQ(requestMsg, "myName: \"Test\"\n");
}

TEST_F(RpcCommunicatorTest, TestNotifyRemoteClientThrowsRuntimeError) {
    setOutgoingListenerWithException<std::runtime_error>(communicator2);
    try {
        sendHelloNotification(communicator2);
        EXPECT_TRUE(false);
    } catch (const RpcClientError& ex) {
        verifyClientError(ex, ErrorCode::RPC_CLIENT_ERROR, "TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestNotifyRemoteClientThrowsFailedToParse) {
    setOutgoingListenerWithException<FailedToParse>(communicator2);
    try {
        sendHelloNotification(communicator2);
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

} // namespace streamr::protorpc
