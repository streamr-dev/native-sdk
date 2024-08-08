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
#include "TestProtos.client.pb.h"
#include "WakeUpRpc.client.pb.h"
#include "WakeUpRpc.server.pb.h"
#include "streamr-proto-rpc/ProtoCallContext.hpp"

// NOLINTBEGIN

namespace streamr::protorpc {

template <typename T>
void setOutgoingListenerWithException(RpcCommunicator& sender) {
    sender.setOutgoingMessageListener(
        [](const RpcMessage& message,
           const std::string& /* requestId */,
           const ProtoCallContext& /* context */) -> void {
            SLogger::info("onOutgoingMessageListener() throws");
            throw T("TestException");
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

void verifyClientError(
    const RpcClientError& ex,
    const ErrorCode expectedErrorCode,
    const std::string expectedOriginalErrorInfo) {
    SLogger::info("Caught RpcClientError", ex.what());
    EXPECT_EQ(ex.code, expectedErrorCode);
    EXPECT_TRUE(ex.originalErrorInfo.has_value());
    EXPECT_EQ(ex.originalErrorInfo.value(), expectedOriginalErrorInfo);
}

struct WakeUpCalled : public Event<std::string_view> {};
using WakeUpEvents = std::tuple<WakeUpCalled>;

class WakeUpRpcServiceImpl : public WakeUpRpcService,
                             public EventEmitter<WakeUpEvents> {
public:
    void wakeUp(
        const WakeUpRequest& request,
        const ProtoCallContext& callContext) override {
        this->emit<WakeUpCalled>(request.reason());
    }
};

class ProtoRpcClientTest : public ::testing::Test {
protected:
    RpcCommunicator communicator1;
    RpcCommunicator communicator2;
    void SetUp() override {}
};

void registerTestRcpMethod(RpcCommunicator& communicator) {
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

void registerTestRcpMethodWithOptionalFields(RpcCommunicator& communicator) {
    communicator.registerRpcMethod<OptionalRequest, OptionalResponse>(
        "getOptional",
        [](const OptionalRequest& request,
           const ProtoCallContext& /* context */) -> OptionalResponse {
            OptionalResponse response;
            return response;
        });
}

TEST_F(ProtoRpcClientTest, TestCanMakeRpcCall) {
    registerTestRcpMethod(communicator1);
    setOutgoingListeners(communicator1, communicator2);
    HelloRpcServiceClient client(communicator2);
    HelloRequest request;
    request.set_myname("Test");
    auto result =
        folly::coro::blockingWait(client.sayHello(request, ProtoCallContext()));
    EXPECT_EQ("Hello, Test", result.greeting());
}

TEST_F(ProtoRpcClientTest, TestCanMakeRpcNotification) {
    WakeUpRpcServiceImpl wakeUpService;
    using namespace std::placeholders;
    communicator1.registerRpcNotification<WakeUpRequest>(
        "wakeUp",
        std::bind(&WakeUpRpcServiceImpl::wakeUp, &wakeUpService, _1, _2));
    setOutgoingListener(communicator2, communicator1);
    std::string reasonResult;
    wakeUpService.on<WakeUpCalled>(
        [&reasonResult](std::string_view reason) -> void {
            reasonResult = std::string(reason);
            SLogger::info("wakeUpService: called with", reason);
        });
    WakeUpRpcServiceClient client(communicator2);
    WakeUpRequest request;
    request.set_reason("School");
    folly::coro::blockingWait(client.wakeUp(request, ProtoCallContext()));
    EXPECT_EQ("School", reasonResult);
}

TEST_F(ProtoRpcClientTest, TestCanMakeRpcCallWithOptionalFields) {
    registerTestRcpMethodWithOptionalFields(communicator1);
    setOutgoingListeners(communicator1, communicator2);
    OptionalServiceClient client(communicator2);
    OptionalRequest request;
    request.set_someoptionalfield("something");
    auto result = folly::coro::blockingWait(
        client.getOptional(request, ProtoCallContext()));
    EXPECT_EQ(false, result.has_someoptionalfield());
}

TEST_F(
    ProtoRpcClientTest,
    TestHandlesClientSideExceptionOnRPCCallsWithRuntimeError) {
    registerTestRcpMethod(communicator1);
    setOutgoingListenerWithException<std::runtime_error>(communicator2);
    HelloRpcServiceClient client(communicator2);
    HelloRequest request;
    request.set_myname("Test");
    try {
        folly::coro::blockingWait(client.sayHello(request, ProtoCallContext()));
        // Test fails here
        EXPECT_TRUE(false);
    } catch (const RpcClientError& ex) {
        verifyClientError(ex, ErrorCode::RPC_CLIENT_ERROR, "TestException");
    } catch (...) {
        EXPECT_TRUE(false);
    }
}

} // namespace streamr::protorpc

// NOLINTEND