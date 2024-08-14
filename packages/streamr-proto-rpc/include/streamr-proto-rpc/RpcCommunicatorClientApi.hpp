#ifndef STREAMR_PROTO_RPC_RPC_COMMUNICATOR_CLIENT_API_HPP
#define STREAMR_PROTO_RPC_RPC_COMMUNICATOR_CLIENT_API_HPP

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <folly/coro/DetachOnCancel.h>
#include <folly/coro/Promise.h>
#include <folly/coro/Task.h>
#include <folly/coro/Timeout.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include "Errors.hpp"
#include "ProtoCallContext.hpp"
#include "ProtoRpc.pb.h"
#include "streamr-logger/SLogger.hpp"
namespace streamr::protorpc {

using google::protobuf::Any;
using streamr::logger::SLogger;
using RpcMessage = ::protorpc::RpcMessage;
using RpcErrorType = ::protorpc::RpcErrorType;
using folly::coro::Task;
class RpcCommunicatorClientApi {
public:
    using OutgoingMessageCallbackType = std::function<void(
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
                    SLogger::trace(
                        "rejectRequest()",
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

    OutgoingMessageCallbackType mOutgoingMessageCallback;
    std::unordered_map<std::string, std::shared_ptr<OngoingRequestBase>>
        mOngoingRequests;
    std::recursive_mutex mOngoingRequestsMutex;
    size_t mRpcRequestTimeout;
    folly::CPUThreadPoolExecutor* mExecutor;

public:
    explicit RpcCommunicatorClientApi(size_t rpcRequestTimeout)
        : mRpcRequestTimeout(rpcRequestTimeout) {
        mExecutor = new folly::CPUThreadPoolExecutor(29); // NOLINT
    }
    ~RpcCommunicatorClientApi() { delete mExecutor; }

    template <typename F>
        requires std::is_assignable_v<OutgoingMessageCallbackType, F>
    void setOutgoingMessageCallback(F&& callback) {
        mOutgoingMessageCallback = std::forward<F>(callback);
    }

    void onIncomingMessage(
        const RpcMessage& rpcMessage, const ProtoCallContext& /* callContext */) {
        std::lock_guard lock(mOngoingRequestsMutex);

        const auto& header = rpcMessage.header();
        if (header.find("response") != header.end()) {
            SLogger::trace("onIncomingMessage() message is a response");
            if (mOngoingRequests.find(rpcMessage.requestid()) !=
                mOngoingRequests.end()) {
                SLogger::trace("onIncomingMessage() ongoing request found");
                if (rpcMessage.has_errortype()) {
                    SLogger::trace(
                        "onIncomingMessage() rejecting ongoing request");
                    this->rejectOngoingRequest(rpcMessage);
                } else {
                    SLogger::trace(
                        "onIncomingMessage() resolving ongoing request");
                    this->resolveOngoingRequest(rpcMessage);
                }
            } else {
                SLogger::trace(
                    "onIncomingMessage() no ongoing request found for requestId, probably the request has timed out");
            }
        } else {
            SLogger::error(
                "onIncomingMessage() a message that is not a response was sent to RpcCommunicatorClientApi");
        }
    }

    // This method is for making RPC calls only, not notifications
    template <typename ReturnType, typename RequestType>
    Task<ReturnType> callRemote(
        const std::string& methodName,
        const RequestType& methodParam,
        const ProtoCallContext& callContext) {
        SLogger::trace("callRemote(): methodName:", methodName);

        size_t timeout = mRpcRequestTimeout;
        if (callContext.timeout.has_value()) {
            timeout = callContext.timeout.value();
        }

        auto requestMessage =
            this->createRequestRpcMessage(methodName, methodParam);

        auto task = folly::coro::co_invoke(
            [requestMessage, callContext, timeout, this]()
                -> folly::coro::Task<ReturnType> {
                auto callMakingTask = folly::coro::co_invoke(
                    [requestMessage,
                     callContext,
                     this]() -> folly::coro::Task<ReturnType> {
                        auto ongoingRequest = this->makeRpcCall<ReturnType>(
                            requestMessage, callContext);
                        co_return co_await std::move(
                            ongoingRequest->getFuture());
                    });

                try {
                    co_return co_await folly::coro::timeout(
                        folly::coro::detachOnCancel(
                            std::move(callMakingTask)
                                .scheduleOn(this->mExecutor)),
                        std::chrono::milliseconds(timeout));
                } catch (const folly::FutureTimeout& e) {
                    SLogger::trace(
                        "callRemote() caught folly::FutureTimeout", e.what());
                    std::lock_guard lock(mOngoingRequestsMutex);
                    mOngoingRequests.erase(requestMessage.requestid());
                    throw RpcTimeout("RPC call timed out");
                } catch (...) {
                    SLogger::trace("callRemote() caught other exception");
                    std::lock_guard lock(mOngoingRequestsMutex);
                    mOngoingRequests.erase(requestMessage.requestid());
                    throw;
                }
            });

        return task;
    }

    Task<void> doNotifyTask(
        RpcMessage requestMessage,
        ProtoCallContext callContext,
        folly::coro::Promise<void>&& promise) {
        try {
            mOutgoingMessageCallback(
                requestMessage, requestMessage.requestid(), callContext);
        } catch (const std::exception& clientSideException) {
            SLogger::debug(
                "Error when calling outgoing message callback from client for sending notification",
                clientSideException.what());
            throw RpcClientError(
                "Error when calling outgoing message callback from client for sending notification",
                clientSideException.what());
        }
        promise.setValue();
        co_return;
    }

    template <typename RequestType>
    Task<void> notifyRemote(
        const std::string_view methodName,
        const RequestType& methodParam,
        const ProtoCallContext& callContext) {
        SLogger::trace("notifyRemote() methodName:", methodName);

        size_t timeout = mRpcRequestTimeout;
        if (callContext.timeout.has_value()) {
            timeout = callContext.timeout.value();
        }

        SLogger::trace(
            "notifyRemote() creating request message, methodName:", methodName);
        auto requestMessage =
            this->createRequestRpcMessage(methodName, methodParam, true);
        auto&& promiseContract = folly::coro::makePromiseContract<void>();

        try {
            mExecutor->add(
                [requestMessage,
                 callContext,
                 promise = std::move(promiseContract.first),
                 outgoingMessageCallback =
                     std::move(mOutgoingMessageCallback)]() mutable -> void {
                    try {
                        outgoingMessageCallback(
                            requestMessage,
                            requestMessage.requestid(),
                            callContext);
                        promise.setValue();
                    } catch (const std::exception& clientSideException) {
                        SLogger::debug(
                            "Error when calling outgoing message callback from client for sending notification",
                            clientSideException.what());
                        promise.setException(RpcClientError(
                            "Error when calling outgoing message callback from client for sending notification",
                            clientSideException.what()));
                    }
                });
            co_return co_await folly::coro::timeout(
                folly::coro::detachOnCancel(std::move(promiseContract.second))
                    .scheduleOn(co_await folly::coro::co_current_executor),
                std::chrono::milliseconds(timeout));
        } catch (const folly::FutureTimeout& e) {
            SLogger::trace(
                "notifyRemote() caught folly::FutureTimeout", e.what());
            throw RpcTimeout("RPC notification timed out");
        } catch (...) {
            SLogger::trace("notifyRemote() caught other exception");
            throw;
        }
    }

    void handleClientError(
        const std::string& requestId, const RpcException& error) {
        std::lock_guard lock(mOngoingRequestsMutex);
        const auto& ongoingRequest = mOngoingRequests.at(requestId);

        if (ongoingRequest) {
            ongoingRequest->rejectRequest(error);
            mOngoingRequests.erase(requestId);
        }
    }

    template <typename ReturnType>
    std::shared_ptr<OngoingRequest<ReturnType>> makeRpcCall(
        const RpcMessage& requestMessage, const ProtoCallContext& callContext) {
        auto ongoingRequest = std::make_shared<OngoingRequest<ReturnType>>();
        {
            std::lock_guard lock(mOngoingRequestsMutex);
            this->mOngoingRequests.emplace(
                requestMessage.requestid(), ongoingRequest);
        }
        if (mOutgoingMessageCallback) {
            try {
                mOutgoingMessageCallback(
                    requestMessage, requestMessage.requestid(), callContext);
            } catch (const std::exception& clientSideException) {
                std::lock_guard lock(mOngoingRequestsMutex);
                if (mOngoingRequests.find(requestMessage.requestid()) !=
                    mOngoingRequests.end()) {
                    SLogger::debug(
                        "Error when calling outgoing message callback from client",
                        clientSideException.what());
                    RpcClientError error(
                        "Error when calling outgoing message callback from client",
                        clientSideException.what());

                    SLogger::debug("Old exception:", error.originalErrorInfo);

                    this->handleClientError(requestMessage.requestid(), error);
                }
            }
        }
        return ongoingRequest;
    }

private:
    template <typename RequestType>
    RpcMessage createRequestRpcMessage(
        const std::string_view methodName,
        const RequestType& request,
        bool notification = false) {
        RpcMessage ret;
        const auto& header = ret.mutable_header();
        header->insert({"request", "request"});
        header->insert({"method", std::string(methodName)});
        SLogger::trace("createRequestRpcMessage() methodName:", methodName);
        if (notification) {
            header->insert({"notification", "notification"});
        }
        Any* body = new Any();
        body->PackFrom(request);
        ret.set_allocated_body(body); // protobuf will take ownership
        SLogger::trace(
            "createRequestRpcMessage() printed request Any: ",
            body->DebugString());
        boost::uuids::uuid uuid;
        ret.set_requestid(boost::uuids::to_string(uuid));
        return ret;
    }

    void resolveOngoingRequest(const RpcMessage& response) {
        std::lock_guard lock(mOngoingRequestsMutex);
        auto& ongoingRequest = mOngoingRequests.at(response.requestid());
        ongoingRequest->resolveRequest(response);
        mOngoingRequests.erase(response.requestid());
    }

    void rejectOngoingRequest(const RpcMessage& response) {
        SLogger::trace("rejectOngoingRequest()", response.DebugString());
        std::lock_guard lock(mOngoingRequestsMutex);

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
#endif // STREAMR_PROTO_RPC_RPC_COMMUNICATOR_CLIENT_API_HPP