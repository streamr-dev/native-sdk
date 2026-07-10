// Module streamr.protorpc.RpcCommunicatorServerApi
// CONSOLIDATED from the former header
// streamr-proto-rpc/RpcCommunicatorServerApi.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
//
// Async request handling (follow-up to Phase A4): incoming requests are
// handled off the delivery thread. onIncomingMessage() copies everything
// the response needs (the resolved handler, the outgoing callback, the
// request message and call context — no `this`), builds a Task<void> that
// awaits the handler and sends the response, and schedules it on the shared
// worker pool via a CancellableAsyncScope, returning immediately. A
// handler may therefore co_await a worker (e.g. the DHT routing worker)
// and SUSPEND the response coroutine instead of blocking the shared
// delivery thread. Notifications remain synchronous/inline.
module;

// std::coroutine_traits must be visible in every translation unit that
// defines OR instantiates a coroutine; it cannot arrive through an
// imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <google/protobuf/any.pb.h>
#include <magic_enum/magic_enum.hpp>
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"

export module streamr.protorpc.RpcCommunicatorServerApi;

import streamr.utils.CoroutineHelper;
import streamr.utils.ExecutorHelper;
import streamr.utils.SharedExecutors;
import streamr.protorpc.Errors;
import streamr.protorpc.ServerRegistry;
export namespace streamr::protorpc {
using RpcMessage = ::protorpc::RpcMessage;
using RpcErrorType = ::protorpc::RpcErrorType;

template <typename CallContextType, typename OutgoingMessageCallbackType>
class RpcCommunicatorServerApi {
private:
    using AsyncHandler =
        std::function<folly::coro::Task<Any>(Any, CallContextType)>;

    ServerRegistry<CallContextType> mServerRegistry;
    OutgoingMessageCallbackType mOutgoingMessageCallback;
    // A per-instance SERIAL view of the shared worker pool preserves the
    // previous serial handling of incoming requests (formerly a private
    // single-thread executor — one dedicated thread per communicator did
    // not scale to hundreds of nodes in one process, see
    // streamr.utils.SharedExecutors). The request coroutines are owned by
    // mScope and drained in the destructor so none outlives this object.
    streamr::utils::SharedSerialExecutor mSerialExecutor{
        streamr::utils::SharedExecutors::worker()};
    folly::coro::CancellableAsyncScope mScope;

    struct RpcResponseParams {
        RpcMessage request;
        std::optional<Any> body;
        std::optional<::protorpc::RpcErrorType> errorType;
        std::optional<std::string> errorClassName;
        std::optional<std::string> errorCode;
        std::optional<std::string> errorMessage;
    };

    static RpcMessage createResponseRpcMessage(
        const RpcResponseParams& params) {
        SLogger::trace("createResponseRpcMessage()");
        RpcMessage ret;

        if (params.body.has_value()) {
            SLogger::trace(
                "createResponseRpcMessage() body has value",
                params.body->DebugString());
            auto* body = new Any(params.body.value());
            ret.set_allocated_body(body); // protobuf will take ownership
        }

        ret.mutable_header()->insert({"response", "response"});
        ret.mutable_header()->insert(
            {"method", params.request.header().at("method")});

        ret.set_requestid(params.request.requestid());

        if (params.errorType.has_value()) {
            ret.set_errortype(params.errorType.value());
        }
        if (params.errorClassName.has_value()) {
            ret.set_errorclassname(params.errorClassName.value());
        }
        if (params.errorCode.has_value()) {
            ret.set_errorcode(params.errorCode.value());
        }
        if (params.errorMessage.has_value()) {
            ret.set_errormessage(params.errorMessage.value());
        }

        return ret;
    }

    // Handles one request and sends its response. Runs on the serial
    // executor and may
    // suspend when the handler co_awaits a worker; every input is held by
    // value in the coroutine frame, so nothing here depends on the delivery
    // thread's stack or on `this` staying alive between suspension points.
    // A detached coroutine must never let an exception escape, so unknown
    // throws are mapped to SERVER_ERROR just like std::exception.
    static folly::coro::Task<void> makeResponseTask(
        std::optional<AsyncHandler> handler,
        OutgoingMessageCallbackType outgoingMessageCallback,
        RpcMessage rpcMessage,
        CallContextType callContext) {
        RpcMessage response;
        try {
            SLogger::trace("handleRequest()");
            if (!handler.has_value()) {
                const auto& header = rpcMessage.header();
                throw UnknownRpcMethod(
                    "RPC Method " +
                    (header.contains("method") ? header.at("method")
                                               : std::string("<missing>")) +
                    " is not provided");
            }
            Any bytes =
                co_await handler.value()(rpcMessage.body(), callContext);
            response = createResponseRpcMessage(
                {.request = rpcMessage, .body = bytes});
        } catch (const Err& err) {
            SLogger::debug("handleRequest() exception ", err.what());
            RpcResponseParams errorParams = {.request = rpcMessage};
            if (err.code == ErrorCode::UNKNOWN_RPC_METHOD) {
                errorParams.errorType = RpcErrorType::UNKNOWN_RPC_METHOD;
            } else if (err.code == ErrorCode::RPC_TIMEOUT) {
                errorParams.errorType = RpcErrorType::SERVER_TIMEOUT;
            } else {
                errorParams.errorType = RpcErrorType::SERVER_ERROR;
                errorParams.errorClassName = typeid(err).name();
                errorParams.errorCode = magic_enum::enum_name(err.code);
                errorParams.errorMessage = err.what();
            }
            SLogger::trace(
                "handleRequest() creating response message for error");
            response = createResponseRpcMessage(errorParams);
        } catch (const std::exception& err) {
            SLogger::debug(
                "Non-RpcCommunicator error when handling request", err.what());
            RpcResponseParams errorParams = {.request = rpcMessage};
            errorParams.errorType = RpcErrorType::SERVER_ERROR;
            errorParams.errorClassName = typeid(err).name();
            errorParams.errorCode =
                magic_enum::enum_name(ErrorCode::RPC_SERVER_ERROR);
            errorParams.errorMessage = err.what();
            response = createResponseRpcMessage(errorParams);
        } catch (...) {
            SLogger::debug("Unknown error when handling request");
            RpcResponseParams errorParams = {.request = rpcMessage};
            errorParams.errorType = RpcErrorType::SERVER_ERROR;
            errorParams.errorCode =
                magic_enum::enum_name(ErrorCode::RPC_SERVER_ERROR);
            response = createResponseRpcMessage(errorParams);
        }

        if (outgoingMessageCallback) {
            try {
                outgoingMessageCallback(
                    response, response.requestid(), callContext);
            } catch (const std::exception& clientSideException) {
                SLogger::debug(
                    "error when calling outgoing message callback from server",
                    clientSideException.what());
            }
        }
        co_return;
    }

    // Resolves the handler on the delivery thread (while `this` is alive),
    // then schedules the response coroutine on the serial executor and
    // returns
    // immediately, so the delivery thread is never blocked by handler work.
    void handleRequest(
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        auto handler = mServerRegistry.getAsyncHandler(rpcMessage);
        mScope.add(
            streamr::utils::co_withExecutor(
                &mSerialExecutor,
                makeResponseTask(
                    std::move(handler),
                    mOutgoingMessageCallback,
                    rpcMessage,
                    callContext)));
    }

    void handleNotification(
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        try {
            mServerRegistry.handleNotification(rpcMessage, callContext);
        } catch (const std::exception& err) {
            SLogger::debug("error", err.what());
        }
    }

public:
    RpcCommunicatorServerApi() = default;

    // Drains any in-flight response coroutines before the registry and the
    // registry are destroyed, so no detached coroutine outlives the state it
    // uses. Must run on a thread other than the serial executor's current
    // worker (it always
    // does: the communicator is owned and destroyed by the transport/main
    // thread, never from inside a request handler).
    ~RpcCommunicatorServerApi() {
        try {
            streamr::utils::blockingWait(mScope.cancelAndJoinAsync());
        } catch (...) { // NOLINT(bugprone-empty-catch) dtor must not throw
        }
    }

    RpcCommunicatorServerApi(const RpcCommunicatorServerApi&) = delete;
    RpcCommunicatorServerApi& operator=(const RpcCommunicatorServerApi&) =
        delete;
    RpcCommunicatorServerApi(RpcCommunicatorServerApi&&) = delete;
    RpcCommunicatorServerApi& operator=(RpcCommunicatorServerApi&&) = delete;

    void onIncomingMessage(
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        const auto& header = rpcMessage.header();

        if (header.find("request") != header.end() &&
            header.find("method") != header.end()) {
            SLogger::trace("onIncomingMessage() message is a request");
            if (header.find("notification") != header.end()) {
                SLogger::trace(
                    "onIncomingMessage() calling handleNotification()");
                this->handleNotification(rpcMessage, callContext);
                SLogger::trace(
                    "onIncomingMessage() handleNotification() called");

            } else {
                SLogger::trace("onIncomingMessage() calling handleRequest()");
                this->handleRequest(rpcMessage, callContext);
            }
        } else {
            SLogger::error(
                "onIncomingMessage() a message that is not a request was sent to RpcCommunicatorServerApi");
        }
    }

    void setOutgoingMessageCallback(OutgoingMessageCallbackType callback) {
        mOutgoingMessageCallback = std::move(callback);
    }

    template <typename RequestType, typename ReturnType, typename F>
        requires std::is_assignable_v<
            std::function<ReturnType(RequestType, CallContextType)>,
            F>
    void registerRpcMethod(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mServerRegistry.template registerRpcMethod<RequestType, ReturnType, F>(
            name, fn, options);
    }

    template <typename RequestType, typename ReturnType, typename F>
        requires std::is_assignable_v<
            std::function<
                folly::coro::Task<ReturnType>(RequestType, CallContextType)>,
            F>
    void registerRpcMethodAsync(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mServerRegistry
            .template registerRpcMethodAsync<RequestType, ReturnType, F>(
                name, fn, options);
    }

    template <typename RequestType, typename F>
        requires std::is_assignable_v<
            std::function<void(RequestType, CallContextType)>,
            F>
    void registerRpcNotification(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mServerRegistry.template registerRpcNotification<RequestType, F>(
            name, fn, options);
    }
};

} // namespace streamr::protorpc
