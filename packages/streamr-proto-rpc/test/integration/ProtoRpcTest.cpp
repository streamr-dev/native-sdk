#include <future>
#include <string>
#include <gtest/gtest.h>
#include <folly/experimental/coro/BlockingWait.h>
#include "HelloRpc.client.pb.h"
#include "TestProtos.client.pb.h"
#include "WakeUpRpc.client.pb.h"
#include "WakeUpRpc.server.pb.h"

import streamr.protorpc;
import streamr.logger;
import streamr.eventemitter;

namespace streamr::protorpc {

using streamr::logger::SLogger;
using RpcCommunicatorType = RpcCommunicator<ProtoCallContext>;

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

void verifyClientError(
    const RpcClientError& ex,
    const ErrorCode expectedErrorCode,
    const std::string& expectedOriginalErrorInfo) {
    SLogger::info("Caught RpcClientError", ex.what());
    EXPECT_EQ(ex.code, expectedErrorCode);
    EXPECT_TRUE(ex.originalErrorInfo.has_value());
    EXPECT_EQ(ex.originalErrorInfo.value(), expectedOriginalErrorInfo);
}
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;

struct WakeUpCalled : public Event<std::string> {};
using WakeUpEvents = std::tuple<WakeUpCalled>;

class WakeUpRpcServiceImpl : public WakeUpRpcService<ProtoCallContext>,
                             public EventEmitter<WakeUpEvents> {
public:
    void wakeUp(
        const WakeUpRequest& request,
        const ProtoCallContext& /* callContext */) override {
        this->emit<WakeUpCalled>(request.reason());
    }
};

class ProtoRpcClientTest : public ::testing::Test {
protected:
    RpcCommunicatorType communicator1; // NOLINT
    RpcCommunicatorType communicator2; // NOLINT

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
        "sayHello",
        [](const HelloRequest& request,
           const ProtoCallContext& /* context */) -> HelloResponse {
            HelloResponse response;
            SLogger::info(
                "registerTestRcpMethod request.myname():", request.myname());
            response.set_greeting("Hello, " + request.myname());
            return response;
        });
}

void registerTestRcpMethodWithOptionalFields(
    RpcCommunicatorType& communicator) {
    communicator.registerRpcMethod<OptionalRequest, OptionalResponse>(
        "getOptional",
        [](const OptionalRequest& /* request */,
           const ProtoCallContext& /* context */) -> OptionalResponse {
            OptionalResponse response;
            return response;
        });
}

TEST_F(ProtoRpcClientTest, TestCanMakeRpcCall) {
    registerTestRcpMethod(communicator1);
    setCallbacks();
    HelloRpcServiceClient<ProtoCallContext> client(communicator2);
    HelloRequest request;
    request.set_myname("Test");
    auto result = folly::coro::blockingWait(
        client.sayHello(std::move(request), ProtoCallContext()));
    EXPECT_EQ("Hello, Test", result.greeting());
}

TEST_F(ProtoRpcClientTest, TestCanMakeRpcNotification) {
    WakeUpRpcServiceImpl wakeUpService;
    communicator1.registerRpcNotification<WakeUpRequest>(
        "wakeUp",
        [&wakeUpService](
            const WakeUpRequest& request, const ProtoCallContext& context) {
            wakeUpService.wakeUp(request, context);
        });
    setCallbacks(false);
    std::promise<std::string> promise;
    wakeUpService.on<WakeUpCalled>(
        [&promise](const std::string& reason) -> void {
            SLogger::info("wakeUpService: called with", reason);
            promise.set_value(reason);
        });
    WakeUpRpcServiceClient<ProtoCallContext> client(communicator2);
    WakeUpRequest request;
    request.set_reason("School");
    folly::coro::blockingWait(
        client.wakeUp(std::move(request), ProtoCallContext()));
    EXPECT_EQ("School", promise.get_future().get());
}

TEST_F(ProtoRpcClientTest, TestCanMakeRpcCallWithOptionalFields) {
    registerTestRcpMethodWithOptionalFields(communicator1);
    setCallbacks();
    OptionalServiceClient<ProtoCallContext> client(communicator2);
    OptionalRequest request;
    request.set_someoptionalfield("something");
    auto result = folly::coro::blockingWait(
        client.getOptional(std::move(request), ProtoCallContext()));
    EXPECT_EQ(false, result.has_someoptionalfield());
}

TEST_F(
    ProtoRpcClientTest,
    TestHandlesClientSideExceptionOnRPCCallsWithRuntimeError) {
    registerTestRcpMethod(communicator1);
    setOutgoingCallbackWithException<std::runtime_error>(communicator2);
    HelloRpcServiceClient<ProtoCallContext> client(communicator2);
    HelloRequest request;
    request.set_myname("Test");
    try {
        folly::coro::blockingWait(
            client.sayHello(std::move(request), ProtoCallContext()));
        // Test fails here
        EXPECT_TRUE(false);
    } catch (const RpcClientError& ex) {
        verifyClientError(ex, ErrorCode::RPC_CLIENT_ERROR, "TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

} // namespace streamr::protorpc
