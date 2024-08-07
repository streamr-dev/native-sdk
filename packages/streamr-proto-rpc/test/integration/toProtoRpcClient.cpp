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
#include "streamr-proto-rpc/Errors.hpp"
#include "streamr-proto-rpc/ProtoCallContext.hpp"

// NOLINTBEGIN

namespace streamr::protorpc {

struct WakeUpCalled : public Event<std::string_view> {};
using WakeUpEvents = std::tuple<WakeUpCalled>;


class WakeUpRpcServiceImpl : public WakeUpRpcService, public EventEmitter<WakeUpEvents>  {
    public:
    void wakeUp(const WakeUpRequest& request, const ProtoCallContext& callContext) override {
         this->emit<WakeUpCalled>(request.reason());
    }

};


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

TEST_F(ProtoRpcClientTest, TestCanMakeRpcNotification) {
    RpcCommunicator communicator1;
    WakeUpRpcServiceImpl wakeUpService;
    using namespace std::placeholders;
    communicator1.registerRpcNotification<WakeUpRequest>(
        "wakeUp", 
        std::bind(&WakeUpRpcServiceImpl::wakeUp, &wakeUpService, _1, _2));

    RpcCommunicator communicator2;
    communicator2.setOutgoingMessageListener(
        [&communicator1](
            const RpcMessage& message,
            const std::string&  requestId ,
            const ProtoCallContext&  context ) -> void {
            SLogger::info("communicator2: onOutgoingMessageListener()");
            communicator1.handleIncomingMessage(message, ProtoCallContext());
        });   
    std::string reasonResult;
    wakeUpService.on<WakeUpCalled>([&reasonResult](std::string_view reason) -> void {
        reasonResult = std::string(reason);
        SLogger::info("wakeUpService: called with", reason);  
    });
    WakeUpRpcServiceClient client(communicator2);
    WakeUpRequest request;
    request.set_reason("School");
    folly::coro::blockingWait(client.wakeUp(request, ProtoCallContext()));
    EXPECT_EQ("School", reasonResult);
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

TEST_F(ProtoRpcClientTest, TestCanMakeRpcCallWithOptionalFields) {
    RpcCommunicator communicator1;
    registerTestRcpMethodWithOptionalFields(communicator1);
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

    SLogger::info("TestCanMakeRpcCallWithOptionalFields setOutgoingMessageListeners called");
    OptionalServiceClient client(communicator2);
    OptionalRequest request;
    request.set_someoptionalfield("something");
    auto result = folly::coro::blockingWait(client.getOptional(request, ProtoCallContext()));
    SLogger::info("TestCanMakeRpcCallWithOptionalFields callRemote called");
    EXPECT_EQ(false, result.has_someoptionalfield());
}


} // namespace streamr::protorpc

// NOLINTEND