#ifndef STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP
#define STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP

#include <memory>
#include <optional>
#include <unordered_map>
#include <google/protobuf/any.pb.h>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <folly/experimental/coro/Promise.h>
#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/Timeout.h>

#include <magic_enum.hpp>

#include "Errors.hpp"
#include "ProtoCallContext.hpp"
#include "ProtoRpc.pb.h"
#include "ServerRegistry.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"

#include "streamr-logger/SLogger.hpp"

namespace streamr::protorpc {

using google::protobuf::Any;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using RpcMessage = ::protorpc::RpcMessage;
using RpcErrorType = ::protorpc::RpcErrorType;
using folly::coro::Task;

// NOLINTBEGIN
enum class StatusCode { OK, STOPPED, DEADLINE_EXCEEDED, SERVER_ERROR };
// NOLINTEND

// RpcCommunicator events
template <typename CallContextType>
struct OutgoingMessage
    : Event<RpcMessage, std::string /*requestId*/, CallContextType> {};

template <typename CallContextType = ProtoCallContext>
class RpcCommunicator
    : public EventEmitter<std::tuple<OutgoingMessage<CallContextType>>> {
public:
    using OutgoingMessageListenerType = std::function<void(
        RpcMessage, std::string /*requestId*/, CallContextType)>;

private:
    class OngoingRequestBase {
    public:
        virtual ~OngoingRequestBase() = default;
        virtual void resolveRequest(const RpcMessage& response) = 0;
        virtual void resolveNotification() = 0;
        virtual void rejectRequest(const std::exception& error) = 0;
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

        virtual void resolveRequest(const RpcMessage& response) {
            this->resolvePromise(response);
        }

        virtual void resolveNotification() {
            mPromiseContract.first.setValue();
        }

        virtual void rejectRequest(const std::exception& error) {
            this->rejectPromise(error);
        }

    private:
        void resolvePromise(const RpcMessage& response) {
            try {
                ResultType result;
                response.body().UnpackTo(&result);

                mPromiseContract.first.setValue(result);
            } catch (...) {
                SLogger::debug(
                    "Could not parse response, Failed to parse received response, \
                     network protocol version is probably incompatible");

                auto error = FailedToParse(
                    "Failed to parse received response, network protocol version \
                     is likely incompatible");

                this->rejectPromise(error);
            }
        }

        void rejectPromise(const std::exception& error) {
            mPromiseContract.first.setException(error);
        }
    };

    bool mStopped = false;
    ServerRegistry mServerRegistry;
    std::unordered_map<std::string, std::shared_ptr<OngoingRequestBase>>
        mOngoingRequests;
    OutgoingMessageListenerType mOutgoingMessageListener;

public:
    // Public API for both client and server

    void handleIncomingMessage(
        const RpcMessage& message, CallContextType callContext) {
        if (mStopped) {
            return;
        }
        this->onIncomingMessage(message, callContext);
    }

    // Server-side registration of RPC methods and listeners

    template <typename RequestType, typename ReturnType, typename F>
        requires std::is_assignable<
            std::function<ReturnType(RequestType, CallContextType)>,
            F>::value
    void registerRpcMethod(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mServerRegistry.registerRpcMethod<RequestType, ReturnType, F>(
            name, fn, options);
    }

    template <typename RequestType, typename F>
        requires std::is_assignable<
            std::function<void(RequestType, CallContextType)>,
            F>::value
    void registerRpcNotification(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mServerRegistry.registerRpcNotification<RequestType, F>(
            name, fn, options);
    }

    template <typename F>
        requires std::is_assignable<OutgoingMessageListenerType, F>::value
    void setOutgoingMessageListener(F&& listener) {
        mOutgoingMessageListener = std::forward<F>(listener);
    }

    // Client-side API
    // This method is for making RPC calls only, not notifications
    template <typename ReturnType, typename RequestType>
    Task<ReturnType> callRemote(
        const std::string& methodName,
        const RequestType& methodParam,
        const CallContextType& callContext) {
        SLogger::info("callRemote()");
        auto task = folly::coro::co_invoke(
            [&methodName, &methodParam, &callContext, this]()
                -> folly::coro::Task<ReturnType> {
                SLogger::info("callRemote() 1");
                
                auto requestMessage =
                    this->createRequestRpcMessage(methodName, methodParam);
                
                auto ongoingRequest =
                    std::make_shared<OngoingRequest<ReturnType>>();
                this->mOngoingRequests.emplace(requestMessage.requestid(), ongoingRequest);
                
                this->template emit<OutgoingMessage<CallContextType>>(
                    requestMessage, requestMessage.requestid(), callContext);

                if (mOutgoingMessageListener) {
                    try {
                        mOutgoingMessageListener(
                            requestMessage,
                            requestMessage.requestid(),
                            callContext);
                    } catch (const std::exception& clientSideException) {
                        if (mOngoingRequests.find(requestMessage.requestid()) !=
                            mOngoingRequests.end()) {
                            this->handleClientError(
                                requestMessage.requestid(),
                                clientSideException);
                        }
                    }
                }

                auto reply = co_await std::move(ongoingRequest->getFuture());
                co_return reply;
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

    static RpcMessage createResponseRpcMessage(
        const RpcResponseParams& params) {

        SLogger::info("createResponseRpcMessage()");
        params.body.value().PrintDebugString();
        SLogger::info("createResponseRpcMessage() 1");
        RpcMessage ret;

        if (params.body.has_value()) {
            SLogger::info("createResponseRpcMessage() body has value");
            auto* body = new Any(params.body.value());
            ret.set_allocated_body(body);   // protobuf will take ownership
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
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        SLogger::debug("onIncomingMessage", rpcMessage.requestid());
        rpcMessage.PrintDebugString();
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
        } else if (
            SLogger::info("onIncomingMessage() 'response not 'found in msg");
            header.find("request") != header.end() &&
            header.find("method") != header.end()) {
            if (header.find("notification") != header.end()) {
                this->handleNotification(rpcMessage, callContext);
            } else {
                this->handleRequest(rpcMessage, callContext);
            }
        }
    }

    void handleRequest(
        const RpcMessage& rpcMessage, CallContextType callContext) {
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
            SLogger::info("handleRequest() creating response message for error");
            response = this->createResponseRpcMessage(errorParams);
        }
        SLogger::info("handleRequest() emitting outgoing message");
        this->template emit<OutgoingMessage<CallContextType>>(
            response, response.requestid(), callContext);

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
        const RpcMessage& rpcMessage, CallContextType callContext) {
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
        const std::string& requestId, const std::exception& error) {
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
        if (notification) {
            header->insert({"notification", "notification"});
        }
        Any* body = new Any();
        body->PackFrom(request);
        ret.set_allocated_body(body);   // protobuf will take ownership
        SLogger::info("createRequestRpcMessage() printed request Any:");
        body->PrintDebugString();

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

        const auto& ongoingRequest = mOngoingRequests.at(response.requestid());

        const auto& header = response.header();

        std::unique_ptr<Err> error;
        if (response.errortype() == RpcErrorType::SERVER_TIMEOUT) {
            error = std::make_unique<RpcTimeout>("Server timed out on request");
        } else if (response.errortype() == RpcErrorType::UNKNOWN_RPC_METHOD) {
            error = std::make_unique<UnknownRpcMethod>(
                "Server does not implement method " + header.at("method"));
        } else if (response.errortype() == RpcErrorType::SERVER_ERROR) {
            error = std::make_unique<RpcServerError>(
                response.errormessage(),
                response.errorclassname(),
                response.errorcode());
        } else {
            error = std::make_unique<RpcRequestError>("Unknown RPC Error");
        }
        ongoingRequest->rejectRequest(*error);
        mOngoingRequests.erase(response.requestid());
    }
};

} // namespace streamr::protorpc

#endif // STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP