#ifndef STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP
#define STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP

#include <chrono>
#include <memory>
#include <optional>
#include <unordered_map>
#include <google/protobuf/any.pb.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <folly/coro/Collect.h>
#include <folly/coro/DetachOnCancel.h>
#include <folly/coro/Timeout.h>
#include <folly/experimental/coro/Collect.h>
#include <folly/experimental/coro/DetachOnCancel.h>
#include <folly/experimental/coro/Promise.h>
#include <folly/experimental/coro/Sleep.h>
#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/Timeout.h>

#include <magic_enum.hpp>

#include "Errors.hpp"
#include "ProtoCallContext.hpp"
#include "ProtoRpc.pb.h"
#include "ServerRegistry.hpp"

#include "streamr-logger/SLogger.hpp"

namespace streamr::protorpc {

using google::protobuf::Any;
using streamr::logger::SLogger;
using RpcMessage = ::protorpc::RpcMessage;
using RpcErrorType = ::protorpc::RpcErrorType;
using folly::coro::Task;

constexpr size_t defaultRpcRequestTimeout = 5000;

// NOLINTBEGIN
enum class StatusCode { OK, STOPPED, DEADLINE_EXCEEDED, SERVER_ERROR };
// NOLINTEND

struct RpcCommunicatorConfig {
    size_t rpcRequestTimeout;
};
class RpcCommunicator {
public:
    using OutgoingMessageListenerType = std::function<void(
        RpcMessage, std::string /*requestId*/, ProtoCallContext)>;

private:
    class OngoingRequestBase {
    public:
        virtual ~OngoingRequestBase() = default;
        virtual void resolveRequest(const RpcMessage& response) = 0;
        virtual void resolveNotification() = 0;
        virtual void rejectRequest(const RpcException& error) = 0;
    };

    template <typename ResultType>
    class OngoingRequest : public OngoingRequestBase {
    private:
        std::pair<
            folly::coro::Promise<ResultType>,
            folly::coro::Future<ResultType>>
            mPromiseContract;

    public:
        OngoingRequest()
            : mPromiseContract(folly::coro::makePromiseContract<ResultType>()) {
        }

        folly::coro::Promise<ResultType>& getPromise() {
            return mPromiseContract.first;
        }

        folly::coro::Future<ResultType>&& getFuture() {
            return std::move(mPromiseContract.second);
        }

        void resolveRequest(const RpcMessage& response) override {
            this->resolvePromise(response);
        }

        void resolveNotification() override {
            mPromiseContract.first.setValue();
        }

        void rejectRequest(const RpcException& error) override {
            std::visit(
                [this](auto&& arg) {
                    SLogger::info(
                        "rejectRequest() ",
                        arg.originalErrorInfo.has_value()
                            ? arg.originalErrorInfo.value()
                            : "");
                    this->mPromiseContract.first.setException(arg);
                },
                error);
        }

    private:
        void resolvePromise(const RpcMessage& response) {
            try {
                ResultType result;
                response.body().UnpackTo(&result);

                mPromiseContract.first.setValue(result);
            } catch (const std::exception& err) {
                SLogger::debug(
                    "Could not parse response, Failed to parse received response, \
                     network protocol version is probably incompatible");

                auto error = FailedToParse(
                    "Failed to parse received response, network protocol version \
                     is likely incompatible");

                mPromiseContract.first.setException(error);
            }
        }
    };

    bool mStopped = false;
    OutgoingMessageListenerType mOutgoingMessageListener;
    ServerRegistry mServerRegistry;
    std::unordered_map<std::string, std::shared_ptr<OngoingRequestBase>>
        mOngoingRequests;
    size_t mRpcRequestTimeout;

public:
    explicit RpcCommunicator(
        std::optional<RpcCommunicatorConfig> config = std::nullopt) {
        if (config.has_value()) {
            mRpcRequestTimeout = config.value().rpcRequestTimeout;
        } else {
            mRpcRequestTimeout = defaultRpcRequestTimeout;
        }
    }
    // Public API for both client and server

    void handleIncomingMessage(
        const RpcMessage& message, const ProtoCallContext& callContext) {
        if (mStopped) {
            return;
        }
        this->onIncomingMessage(message, callContext);
    }

    // Server-side registration of RPC methods and listeners

    template <typename RequestType, typename ReturnType, typename F>
        requires std::is_assignable_v<
            std::function<ReturnType(RequestType, ProtoCallContext)>,
            F>
    void registerRpcMethod(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mServerRegistry.registerRpcMethod<RequestType, ReturnType, F>(
            name, fn, options);
    }

    template <typename RequestType, typename F>
        requires std::is_assignable_v<
            std::function<void(RequestType, ProtoCallContext)>,
            F>
    void registerRpcNotification(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mServerRegistry.registerRpcNotification<RequestType, F>(
            name, fn, options);
    }

    template <typename F>
        requires std::is_assignable_v<OutgoingMessageListenerType, F>
    void setOutgoingMessageListener(F&& listener) {
        mOutgoingMessageListener = std::forward<F>(listener);
    }

    // Client-side API
    // This method is for making RPC calls only, not notifications
    template <typename ReturnType, typename RequestType>
    Task<ReturnType> callRemote(
        const std::string& methodName,
        const RequestType& methodParam,
        const ProtoCallContext& callContext) {
        SLogger::info("callRemote(): methodName:", methodName);

        size_t timeout = mRpcRequestTimeout;
        if (callContext.timeout.has_value()) {
            timeout = callContext.timeout.value();
        }

        auto task = folly::coro::co_invoke(
            [methodName, methodParam, callContext, timeout, this]()
                -> folly::coro::Task<ReturnType> {
                SLogger::info("callRemote() 1: methodName:", methodName);

                auto subtask = folly::coro::co_invoke(
                    [methodName, methodParam, callContext, this]()
                        -> folly::coro::Task<ReturnType> {
                        auto callMakingTask = folly::coro::co_invoke(
                            [methodName, methodParam, callContext, this]()
                                -> folly::coro::Task<ReturnType> {
                                auto ongoingRequest =
                                    this->makeRpcCall<ReturnType, RequestType>(
                                        methodName, methodParam, callContext);
                                co_return co_await std::move(
                                    ongoingRequest->getFuture());
                            });

                        try {
                            co_return co_await folly::coro::detachOnCancel(
                                std::move(callMakingTask)
                                    .scheduleOn(
                                        folly::getGlobalCPUExecutor().get()));
                        } catch (const folly::OperationCancelled&) {
                            SLogger::info("folly::OperationCancelled");
                            throw RpcTimeout("RPC call timed out");
                        }
                    });
                try {
                    co_return co_await folly::coro::timeout(
                        folly::coro::detachOnCancel(std::move(subtask)),
                        std::chrono::milliseconds(timeout));
                } catch (const folly::FutureTimeout& e) {
                    SLogger::info("folly::FutureTimeout2 oli", e.what());
                    throw RpcTimeout("RPC call timed out");
                }
            });

        return task;
    }

    template <typename RequestType>
    Task<void> notifyRemote(
        const std::string& methodName,
        const RequestType& methodParam,
        const ProtoCallContext& callContext) {
        SLogger::info("notifyRemote()");
        auto task = folly::coro::co_invoke(
            [methodName, methodParam, callContext, this]()
                -> folly::coro::Task<void> {
                SLogger::info("notifyRemote() 1");
                auto requestMessage = this->createRequestRpcMessage(
                    methodName, methodParam, true);

                try {
                    mOutgoingMessageListener(
                        requestMessage,
                        requestMessage.requestid(),
                        callContext);
                } catch (const std::exception& clientSideException) {
                    SLogger::debug(
                        "Error when calling outgoing message listener from client",
                        clientSideException.what());
                    throw RpcClientError(
                        "Error when calling outgoing message listener from client",
                        clientSideException.what());
                }
                co_return;
            });
        return std::move(task);
    }

private:
    struct RpcResponseParams {
        RpcMessage request;
        std::optional<Any> body;
        std::optional<::protorpc::RpcErrorType> errorType;
        std::optional<std::string> errorClassName;
        std::optional<std::string> errorCode;
        std::optional<std::string> errorMessage;
    };

    template <typename ReturnType, typename RequestType>
    std::shared_ptr<OngoingRequest<ReturnType>> makeRpcCall(
        const std::string& methodName,
        const RequestType& methodParam,
        const ProtoCallContext& callContext) {
        auto requestMessage =
            this->createRequestRpcMessage(methodName, methodParam);

        auto ongoingRequest = std::make_shared<OngoingRequest<ReturnType>>();
        this->mOngoingRequests.emplace(
            requestMessage.requestid(), ongoingRequest);

        if (mOutgoingMessageListener) {
            try {
                mOutgoingMessageListener(
                    requestMessage, requestMessage.requestid(), callContext);
            } catch (const std::exception& clientSideException) {
                if (mOngoingRequests.find(requestMessage.requestid()) !=
                    mOngoingRequests.end()) {
                    SLogger::debug(
                        "Error when calling outgoing message listener from client",
                        clientSideException.what());
                    RpcClientError error(
                        "Error when calling outgoing message listener from client",
                        clientSideException.what());

                    SLogger::debug("Old exception:", error.originalErrorInfo);

                    this->handleClientError(requestMessage.requestid(), error);
                }
            }
        }
        return ongoingRequest;
    }

    static RpcMessage createResponseRpcMessage(
        const RpcResponseParams& params) {
        SLogger::info("createResponseRpcMessage()");
        RpcMessage ret;

        if (params.body.has_value()) {
            SLogger::info(
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

    void onIncomingMessage(
        const RpcMessage& rpcMessage, const ProtoCallContext& callContext) {
        SLogger::debug("onIncomingMessage", rpcMessage.DebugString());
        SLogger::info("onIncomingMessage() 1");
        const auto& header = rpcMessage.header();
        SLogger::info("onIncomingMessage() 2", header);
        if (header.find("response") != header.end()) {
            SLogger::info("onIncomingMessage() found response in msg");
        } else {
            SLogger::info("onIncomingMessage() 'response' not found in msg");
        }

        SLogger::info("onIncomingMessage() requestId", rpcMessage.requestid());
        SLogger::info("Printing all keys of mOngoingRequests:");
        for (const auto& ongoingRequest : mOngoingRequests) {
            SLogger::info("Key: ", ongoingRequest.first);
        }
        if (header.find("response") != header.end() &&
            mOngoingRequests.find(rpcMessage.requestid()) !=
                mOngoingRequests.end()) {
            SLogger::info("onIncomingMessage() found response in msg");
            if (rpcMessage.has_errortype()) {
                SLogger::info("onIncomingMessage() rejecting ongoing request");
                this->rejectOngoingRequest(rpcMessage);
            } else {
                SLogger::info("onIncomingMessage() resolving ongoing request");
                this->resolveOngoingRequest(rpcMessage);
            }
        } else if (SLogger::info(
                       "onIncomingMessage() 'response not 'found in msg");
                   header.find("request") != header.end() &&
                   header.find("method") != header.end()) {
            if (header.find("notification") != header.end()) {
                SLogger::info(
                    "onIncomingMessage() calling handleNotification()");
                this->handleNotification(rpcMessage, callContext);
            } else {
                SLogger::info("onIncomingMessage() calling handleRequest()");
                this->handleRequest(rpcMessage, callContext);
            }
        }
    }

    void handleRequest(
        const RpcMessage& rpcMessage, const ProtoCallContext& callContext) {
        if (mStopped) {
            return;
        }

        RpcMessage response;
        try {
            SLogger::info("handleRequest() 1");
            auto bytes = mServerRegistry.handleRequest(rpcMessage, callContext);
            SLogger::info("handleRequest() 2", bytes.GetTypeName());
            SLogger::info("handleRequest() creating response message");
            response = RpcCommunicator::createResponseRpcMessage(
                {.request = rpcMessage, .body = bytes});
        } catch (const Err& err) {
            SLogger::info("handleRequest() exception ", err.what());
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
            SLogger::info(
                "handleRequest() creating response message for error");
            response = RpcCommunicator::createResponseRpcMessage(errorParams);
        } catch (const std::exception& err) {
            SLogger::debug(
                "Non-RpcCommunicator error when handling request", err.what());
            RpcResponseParams errorParams = {.request = rpcMessage};
            errorParams.errorType = RpcErrorType::SERVER_ERROR;
            errorParams.errorClassName = typeid(err).name();
            errorParams.errorCode =
                magic_enum::enum_name(ErrorCode::RPC_SERVER_ERROR);
            errorParams.errorMessage = err.what();
            response = RpcCommunicator::createResponseRpcMessage(errorParams);
        }

        if (mOutgoingMessageListener) {
            try {
                mOutgoingMessageListener(
                    response, response.requestid(), callContext);
            } catch (const std::exception& clientSideException) {
                SLogger::debug(
                    "error when calling outgoing message listener from server",
                    clientSideException.what());
            }
        }
    }

    void handleNotification(
        const RpcMessage& rpcMessage, const ProtoCallContext& callContext) {
        if (mStopped) {
            return;
        }
        try {
            mServerRegistry.handleNotification(rpcMessage, callContext);
        } catch (const std::exception& err) {
            SLogger::debug("error", err.what());
        }
    }

    // Client-side functions

    void handleClientError(
        const std::string& requestId, const RpcException& error) {
        if (mStopped) {
            return;
        }

        const auto& ongoingRequest = mOngoingRequests.at(requestId);

        if (ongoingRequest) {
            ongoingRequest->rejectRequest(error);
            mOngoingRequests.erase(requestId);
        }
    }

    template <typename RequestType>
    RpcMessage createRequestRpcMessage(
        const std::string& methodName,
        const RequestType& request,
        bool notification = false) {
        RpcMessage ret;
        const auto& header = ret.mutable_header();
        header->insert({"request", "request"});
        header->insert({"method", methodName});
        SLogger::info("createRequestRpcMessage() methodName:", methodName);
        if (notification) {
            header->insert({"notification", "notification"});
        }
        Any* body = new Any();
        body->PackFrom(request);
        ret.set_allocated_body(body); // protobuf will take ownership
        SLogger::info(
            "createRequestRpcMessage() printed request Any: ",
            body->DebugString());
        boost::uuids::uuid uuid;
        ret.set_requestid(boost::uuids::to_string(uuid));
        return ret;
    }

    void resolveOngoingRequest(const RpcMessage& response) {
        if (mStopped) {
            return;
        }
        auto& ongoingRequest = mOngoingRequests.at(response.requestid());
        ongoingRequest->resolveRequest(response);
        mOngoingRequests.erase(response.requestid());
    }

    void rejectOngoingRequest(const RpcMessage& response) {
        if (mStopped) {
            return;
        }
        SLogger::info("rejectOngoingRequest()", response.DebugString());
        const auto& ongoingRequest = mOngoingRequests.at(response.requestid());

        const auto& header = response.header();

        if (response.errortype() == RpcErrorType::SERVER_TIMEOUT) {
            ongoingRequest->rejectRequest(
                RpcTimeout("Server timed out on request"));
        } else if (response.errortype() == RpcErrorType::UNKNOWN_RPC_METHOD) {
            ongoingRequest->rejectRequest(UnknownRpcMethod(
                "Server does not implement method " + header.at("method")));
        } else if (response.errortype() == RpcErrorType::SERVER_ERROR) {
            ongoingRequest->rejectRequest(RpcServerError(
                response.has_errormessage() ? response.errormessage() : "",
                response.has_errorclassname() ? response.errorclassname() : "",
                response.has_errorcode() ? response.errorcode() : ""));
        } else {
            ongoingRequest->rejectRequest(RpcRequestError("Unknown RPC Error"));
        }
        mOngoingRequests.erase(response.requestid());
    }
};

} // namespace streamr::protorpc

#endif // STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP