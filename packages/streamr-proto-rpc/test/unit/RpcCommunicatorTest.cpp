#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <thread>
#include <gtest/gtest.h>
#include "HelloRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.utils.ExecutorHelper;
import streamr.protorpc.Errors;
import streamr.protorpc.ProtoCallContext;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.RpcCommunicatorClientApi;
import streamr.protorpc.protos;
import streamr.logger.SLogger;

namespace streamr::protorpc {

using namespace std::chrono_literals;
using streamr::logger::SLogger;
using RpcCommunicatorType = RpcCommunicator<ProtoCallContext>;

// The test's own pool for driving calls (formerly reused the client API's
// threadPoolSize constant, which went away with the per-instance pools).
constexpr size_t testThreadPoolSize = 20;

class RpcCommunicatorTest : public ::testing::Test {
public:
    RpcCommunicatorTest()
        : executor(folly::CPUThreadPoolExecutor(testThreadPoolSize)) {
    } // NOLINT

    ~RpcCommunicatorTest() override try {
        SLogger::warn("Deleting executor of RpcCommunicatorTest");
        SLogger::warn("RpcCommunicatorTypeTest executor deleted");
    } catch (...) { // NOLINT(bugprone-empty-catch) dtor must not throw;
                    // body is logging only
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
void setOutgoingCallback(RpcCommunicatorType& sender, RpcCommunicatorType&
receiver) { sender.setOutgoingMessageCallback(
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
    return streamr::utils::blockingWait(
        streamr::utils::co_withExecutor(
            executor,
            sender.request<HelloResponse, HelloRequest>(
                "testFunction", request, ProtoCallContext())));
}

auto sendHelloNotification(
    RpcCommunicatorType& sender, folly::CPUThreadPoolExecutor* executor) {
    HelloRequest request;
    request.set_myname("Test");
    streamr::utils::blockingWait(
        streamr::utils::co_withExecutor(
            executor,
            sender.notify<HelloRequest>(
                "testFunction", request, ProtoCallContext())));
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
        EXPECT_TRUE(ex.errorClassName.contains("runtime_error"));
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
        EXPECT_TRUE(ex.errorClassName.contains("FailedToParse"));
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
            -> void { requestMsg = request.myname(); });
    setCallbacks(false);
    sendHelloNotification(communicator2, &executor);
    EXPECT_EQ(requestMsg, "Test");
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
        streamr::utils::blockingWait(
            streamr::utils::co_withExecutor(
                &executor,
                communicator2.request<HelloResponse, HelloRequest>(
                    "testFunction",
                    request,
                    ProtoCallContext(),
                    50ms))); // NOLINT
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
            // The lambda body is fully wrapped in try/catch; the residual
            // exception-escape finding is a clangd-modules analysis
            // artifact.
            thread = std::make_shared<std::thread>(
                // NOLINTNEXTLINE(bugprone-exception-escape)
                [&communicator1, message, context]() {
                    try {
                        SLogger::info("Starting thread for server");
                        communicator1.handleIncomingMessage(message, context);
                    } catch (...) { // NOLINT(bugprone-empty-catch) an
                                    // exception escaping a thread body
                                    // would std::terminate
                    }
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
        auto result = streamr::utils::blockingWait(
            streamr::utils::co_withExecutor(
                &executor,
                communicator2.request<HelloResponse, HelloRequest>(
                    "testFunction",
                    request,
                    ProtoCallContext(),
                    50ms))); // NOLINT
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
            -> void { requestMsg = request.myname(); });

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
        streamr::utils::blockingWait(
            streamr::utils::co_withExecutor(
                &executor,
                communicator2.notify<HelloRequest>(
                    "testFunction",
                    request,
                    ProtoCallContext(),
                    50ms))); // NOLINT
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

// --- Async server handlers: the delivery thread must not block on the
// handler (follow-up to Phase A4). ---

namespace {
RpcMessage makeRequestMessage(
    const std::string& methodName, const std::string& myname) {
    static std::atomic<uint64_t> counter = 0;
    HelloRequest request;
    request.set_myname(myname);
    RpcMessage message;
    message.mutable_body()->PackFrom(request);
    (*message.mutable_header())["method"] = methodName;
    (*message.mutable_header())["request"] = "request";
    message.set_requestid("req-" + std::to_string(counter++));
    return message;
}
} // namespace

// handleIncomingMessage() delivers on the (shared) delivery thread. When a
// handler is async and slow, the call must return promptly and the response
// must be produced later, off that thread.
TEST_F(RpcCommunicatorTest, TestServerAsyncHandlerDoesNotBlockDeliveryThread) {
    constexpr auto handlerDelay = 500ms;
    std::atomic<bool> responseSent = false;
    communicator1.setOutgoingMessageCallback(
        [&responseSent](
            const RpcMessage& /* message */,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            responseSent = true;
        });
    communicator1.registerRpcMethodAsync<HelloRequest, HelloResponse>(
        "slowFunction",
        [handlerDelay](
            const HelloRequest& request, const ProtoCallContext& /* context */)
            -> folly::coro::Task<HelloResponse> {
            co_await folly::coro::sleep(handlerDelay);
            HelloResponse response;
            response.set_greeting("Hello, " + request.myname());
            co_return response;
        });

    const auto start = std::chrono::steady_clock::now();
    communicator1.handleIncomingMessage(
        makeRequestMessage("slowFunction", "Test"), ProtoCallContext());
    const auto elapsed = std::chrono::steady_clock::now() - start;

    // The delivery thread returned well before the handler's delay, and the
    // response has not been produced yet.
    EXPECT_LT(elapsed, 200ms);
    EXPECT_EQ(responseSent.load(), false);

    // After the handler's delay the response is delivered off-thread.
    std::this_thread::sleep_for(handlerDelay + 400ms);
    EXPECT_EQ(responseSent.load(), true);
}

// Our own (fast) incoming traffic must be serviced while a slow handler is
// still in flight — the priority-1 guarantee the async change restores.
//
// Deterministic (no wall-clock thresholds, robust under CI load): the slow
// handler stays gated until the test releases it, so observing the fast
// response arrive WHILE the slow one is still pending proves our own traffic
// was not stuck behind the in-flight slow handler. The wait bounds only
// guard against a hang; they are not part of the timing being asserted.
TEST_F(RpcCommunicatorTest, TestOwnTrafficNotDelayedByInFlightSlowHandler) {
    constexpr auto pollInterval = 5ms;
    constexpr auto hangGuard = 5s;
    auto released = std::make_shared<std::atomic<bool>>(false);
    std::atomic<int> order = 0;
    std::atomic<int> slowOrder = 0;
    std::atomic<int> fastOrder = 0;
    std::atomic<bool> slowDone = false;
    std::atomic<bool> fastDone = false;
    communicator1.setOutgoingMessageCallback(
        [&](const RpcMessage& message,
            const std::string& /* requestId */,
            const ProtoCallContext& /* context */) -> void {
            if (message.header().at("method") == "slow") {
                slowOrder = ++order;
                slowDone = true;
            } else {
                fastOrder = ++order;
                fastDone = true;
            }
        });
    // A slow async handler (stands in for routing): it stays pending, yielding
    // the worker thread on each poll, until the test releases it.
    communicator1.registerRpcMethodAsync<HelloRequest, HelloResponse>(
        "slow",
        [released, pollInterval](
            const HelloRequest& request, const ProtoCallContext& /* context */)
            -> folly::coro::Task<HelloResponse> {
            while (!released->load()) {
                co_await folly::coro::sleep(pollInterval);
            }
            HelloResponse response;
            response.set_greeting("Hello, " + request.myname());
            co_return response;
        });
    // ... and a fast synchronous handler (stands in for our own ping).
    communicator1.registerRpcMethod<HelloRequest, HelloResponse>(
        "fast",
        [](const HelloRequest& request,
           const ProtoCallContext& /* context */) -> HelloResponse {
            HelloResponse response;
            response.set_greeting("Hello, " + request.myname());
            return response;
        });

    // Deliver the slow request first, then our own fast one right behind it.
    communicator1.handleIncomingMessage(
        makeRequestMessage("slow", "A"), ProtoCallContext());
    communicator1.handleIncomingMessage(
        makeRequestMessage("fast", "B"), ProtoCallContext());

    // The fast response comes back while the slow handler is still gated.
    const auto fastDeadline = std::chrono::steady_clock::now() + hangGuard;
    while (!fastDone.load() &&
           std::chrono::steady_clock::now() < fastDeadline) {
        std::this_thread::sleep_for(pollInterval);
    }
    EXPECT_EQ(fastDone.load(), true);
    EXPECT_EQ(slowDone.load(), false);

    // Releasing the slow handler lets it complete; it responded after us.
    released->store(true);
    const auto slowDeadline = std::chrono::steady_clock::now() + hangGuard;
    while (!slowDone.load() &&
           std::chrono::steady_clock::now() < slowDeadline) {
        std::this_thread::sleep_for(pollInterval);
    }
    EXPECT_EQ(slowDone.load(), true);
    EXPECT_LT(fastOrder.load(), slowOrder.load());
}

} // namespace streamr::protorpc
