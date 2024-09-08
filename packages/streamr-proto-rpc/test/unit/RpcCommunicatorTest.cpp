#include <exception>
#include <thread>
#include <chrono>
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
#include "streamr-proto-rpc/ServerRegistry.hpp"

namespace streamr::protorpc {

using namespace std::chrono_literals;
using RpcCommunicatorType = RpcCommunicator<ProtoCallContext>;
class RpcCommunicatorTest : public ::testing::Test {
public:
    RpcCommunicatorTest()
        : executor(folly::CPUThreadPoolExecutor(threadPoolSize)) {} // NOLINT

    ~RpcCommunicatorTest() override {
        SLogger::warn("Deleting executor of RpcCommunicatorTest");
        SLogger::warn("RpcCommunicatorTypeTest executor deleted");
    }

protected:
    RpcCommunicatorType communicator1; // NOLINT
    RpcCommunicatorType communicator2; // NOLINT
    folly::CPUThreadPoolExecutor executor; // NOLINT

    void SetUp() override {}

    void setCallbacks(bool isMethod = true) {
        communicator2.setOutgoingMessageCallback(
            [this](
                const RpcMessage& message,
                const std::string& /* requestId */,
                const ProtoCallContext& /* context */) -> void {
                SLogger::info("onOutgoingMessageCallback()");
                communicator1.handleIncomingMessage(
                    message, ProtoCallContext());
            });
        if (isMethod) {
            communicator1.setOutgoingMessageCallback(
                [this](
                    const RpcMessage& message,
                    const std::string& /* requestId */,
                    const ProtoCallContext& /* context */) -> void {
                    SLogger::info("onOutgoingMessageCallback()");
                    communicator2.handleIncomingMessage(
                        message, ProtoCallContext());
                });
        }
    }
};

void registerTestRcpMethod(RpcCommunicatorType& communicator) {
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
void registerThrowingRcpMethod(RpcCommunicatorType& communicator) {
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& /* request */,
           const ProtoCallContext& /* context */) -> HelloResponse {
            throw T("TestException");
        });
}

void registerThrowingTestRcpMethodUnknown(RpcCommunicatorType& communicator) {
    const int unknownThrow = 42;
    communicator.registerRpcMethod<HelloRequest, HelloResponse>(
        "testFunction",
        [](const HelloRequest& /* request */,
           const ProtoCallContext& /* context */) -> HelloResponse {
            throw unknownThrow; // NOLINT
        });
}
/*
void setOutgoingCallback(RpcCommunicatorType& sender, RpcCommunicatorType& receiver) {
    sender.setOutgoingMessageCallback(
        [&receiver](
            const RpcMessage& message,
            const std::string& requestId ,
            const ProtoCallContext&  context ) -> void {
            SLogger::info("onOutgoingMessageCallback()");
            receiver.handleIncomingMessage(message, ProtoCallContext());
        });
}

void setOutgoingCallbacks(
    RpcCommunicatorType& communicator1, RpcCommunicatorType& communicator2) {
    setOutgoingCallback(communicator2, communicator1);
    setOutgoingCallback(communicator1, communicator2);
}
*/
template <typename T>
void setOutgoingCallbackWithException(RpcCommunicatorType& sender) {
    sender.setOutgoingMessageCallback(
        [](const RpcMessage& /* message */,
           const std::string& /* requestId */,
           const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageCallback() throws");
            throw T("TestException");
        });
}

void setOutgoingCallbackWithThrownUnknown(RpcCommunicatorType& sender) {
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
    RpcCommunicatorType& sender, folly::CPUThreadPoolExecutor* executor) {
    HelloRequest request;
    request.set_myname("Test");
    return folly::coro::blockingWait(
        sender
            .request<HelloResponse, HelloRequest>(
                "testFunction", request, ProtoCallContext())
            .scheduleOn(executor));
}

auto sendHelloNotification(
    RpcCommunicatorType& sender, folly::CPUThreadPoolExecutor* executor) {
    HelloRequest request;
    request.set_myname("Test");
    folly::coro::blockingWait(
        sender.notify<HelloRequest>("testFunction", request, ProtoCallContext())
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

void registerSleepingTestRcpMethod(RpcCommunicatorType& communicator) {
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
    RpcCommunicatorType communicator;
    registerTestRcpMethod(communicator);
    EXPECT_EQ(true, true);
}

TEST_F(RpcCommunicatorTest, TestCanMakeRpcCall) {
    registerTestRcpMethod(communicator1);
    setCallbacks();
    auto result = sendHelloRequest(communicator2, &executor);
    EXPECT_EQ("Hello, Test", result.greeting());
}

TEST_F(RpcCommunicatorTest, TestrequestClientThrowsRuntimeError) {
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

TEST_F(RpcCommunicatorTest, TestrequestClientThrowsFailedToParse) {
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

TEST_F(RpcCommunicatorTest, TestrequestClientThrowsUnknownError) {
    registerTestRcpMethod(communicator1);
    setOutgoingCallbackWithThrownUnknown(communicator2);
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (...) {
        EXPECT_TRUE(true);
    }
}

TEST_F(RpcCommunicatorTest, TestrequestServerThrowsRuntimeError) {
    registerThrowingRcpMethod<std::runtime_error>(communicator1);
    setCallbacks();
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

TEST_F(RpcCommunicatorTest, TestrequestServerThrowsUnknownRpcMethod) {
    registerThrowingRcpMethod<UnknownRpcMethod>(communicator1);
    setCallbacks();
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const UnknownRpcMethod& ex) {
        EXPECT_EQ(ex.code, ErrorCode::UNKNOWN_RPC_METHOD);
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestrequestServerThrowsRcpTimeout) {
    registerThrowingRcpMethod<RpcTimeout>(communicator1);
    setCallbacks();
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (const RpcTimeout& ex) {
        EXPECT_EQ(ex.code, ErrorCode::RPC_TIMEOUT);
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

TEST_F(RpcCommunicatorTest, TestrequestServerThrowsFailedToParse) {
    registerThrowingRcpMethod<FailedToParse>(communicator1);
    setCallbacks();
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

TEST_F(RpcCommunicatorTest, TestrequestServerThrowsUnknown) {
    registerThrowingTestRcpMethodUnknown(communicator1);
    setCallbacks();
    try {
        sendHelloRequest(communicator2, &executor);
        EXPECT_TRUE(false);
    } catch (...) {
        EXPECT_TRUE(true);
    }
}

TEST_F(RpcCommunicatorTest, TestCannotify) {
    std::string requestMsg;
    communicator1.registerRpcNotification<HelloRequest>(
        "testFunction",
        [&requestMsg](
            const HelloRequest& request, const ProtoCallContext& /* context */)
            -> void { requestMsg = request.DebugString(); });
    setCallbacks(false);
    sendHelloNotification(communicator2, &executor);
    EXPECT_EQ(requestMsg, "myName: \"Test\"\n");
}

TEST_F(RpcCommunicatorTest, TestnotifyClientThrowsRuntimeError) {
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

TEST_F(RpcCommunicatorTest, TestnotifyClientThrowsFailedToParse) {
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
    registerTestRcpMethod(communicator1);

    communicator2.setOutgoingMessageCallback(
        [&communicator1 = this->communicator1](
            const RpcMessage& /* message */,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            SLogger::info("setOutgoingMessageCallback() sleeping 1s");
            std::this_thread::sleep_for(std::chrono::seconds(1)); // NOLINT
        });

    HelloRequest request;
    request.set_myname("Test");

    try {
        folly::coro::blockingWait(
            communicator2
                .request<HelloResponse, HelloRequest>(
                    "testFunction",
                    request,
                    ProtoCallContext{.timeout = 50ms}) // NOLINT
                .scheduleOn(&executor));
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
    registerSleepingTestRcpMethod(communicator1);

    std::shared_ptr<std::thread> thread;

    communicator2.setOutgoingMessageCallback(
        [&communicator1 = this->communicator1, &thread](
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
        [&communicator2 = this->communicator2](
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
                .request<HelloResponse, HelloRequest>(
                    "testFunction",
                    request,
                    ProtoCallContext{.timeout = 50ms}) // NOLINT
                .scheduleOn(&executor));
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
    std::string requestMsg;

    communicator1.registerRpcNotification<HelloRequest>(
        "testFunction",
        [&requestMsg](
            const HelloRequest& request, const ProtoCallContext& /* context */)
            -> void { requestMsg = request.DebugString(); });

    HelloRequest request;
    request.set_myname("Test");

    communicator2.setOutgoingMessageCallback(
        [&communicator1 = this->communicator1](
            const RpcMessage& /* message */,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            std::cout << "setOutgoingMessageCallback() sleeping 1s, thread id: "
                      << std::this_thread::get_id() << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1)); // NOLINT
        });
    try {
        // auto str = std::this_thread::get_id();

        std::cout << "Calling notify() from thread id: "
                  << std::this_thread::get_id() << "\n";
        folly::coro::blockingWait(
            communicator2
                .notify<HelloRequest>(
                    "testFunction",
                    request,
                    ProtoCallContext{.timeout = 50ms}) // NOLINT
                .scheduleOn(&executor));
        // Test fails here
        EXPECT_TRUE(false);
    } catch (const RpcTimeout& ex) {
        std::cout
            << "TestRpcTimeoutOnClientSideForNotification caught RpcTimeout in thread id: "
            << std::this_thread::get_id() << "\n";
    } catch (const std::exception& ex) {
        std::cout
            << "TestRpcTimeoutOnClientSideForNotification caught unknown exception in thread id: "
            << std::this_thread::get_id() << "\n";
        EXPECT_TRUE(false);
        EXPECT_TRUE(false);
    }
}

} // namespace streamr::protorpc
