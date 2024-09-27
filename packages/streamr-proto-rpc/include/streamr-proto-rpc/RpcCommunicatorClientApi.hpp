#ifndef STREAMR_PROTO_RPC_RPC_COMMUNICATOR_CLIENT_API_HPP
#define STREAMR_PROTO_RPC_RPC_COMMUNICATOR_CLIENT_API_HPP

#include <exception>
#include <map>
#include <folly/experimental/coro/DetachOnCancel.h>
#include <folly/experimental/coro/Promise.h>
#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/Timeout.h>
#include <folly/executors/CPUThreadPoolExecutor.h>
#include "Errors.hpp"
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/Branded.hpp"
#include "streamr-utils/Uuid.hpp"
namespace streamr::protorpc {

using google::protobuf::Any;
using streamr::logger::SLogger;
using RpcMessage = ::protorpc::RpcMessage;
using RpcErrorType = ::protorpc::RpcErrorType;
using folly::coro::Task;
using streamr::utils::Branded;
using streamr::utils::Uuid;

constexpr size_t threadPoolSize = 20;

template <typename CallContextType, typename OutgoingMessageCallbackType>
class RpcCommunicatorClientApi {
public:
    using RequestId = Branded<std::string, struct RequestIdTag>;

    class OngoingRequestBase {
    private:
        CallContextType mCallContext;

    public:
        explicit OngoingRequestBase(CallContextType callContext)
            : mCallContext(std::move(callContext)) {}
        virtual ~OngoingRequestBase() = default;
        virtual void resolveRequest(const RpcMessage& response) = 0;
        // virtual void resolveNotification() = 0;
        virtual void rejectRequest(const RpcException& error) = 0;
        virtual bool fulfilsPredicate(
            const std::function<bool(const OngoingRequestBase&)>& predicate)
            const {
            return predicate(*this);
        };
        const CallContextType& getCallContext() const { return mCallContext; }
    };

    using OngoingRequestPredicate =
        std::function<bool(const OngoingRequestBase&)>;

private:
    template <typename ResultType>
    class OngoingRequest : public OngoingRequestBase {
    private:
        std::pair<
            folly::coro::Promise<ResultType>,
            folly::coro::Future<ResultType>>
            mPromiseContract;

    public:
        explicit OngoingRequest(CallContextType callContext)
            : OngoingRequestBase(std::move(callContext)),
              mPromiseContract(folly::coro::makePromiseContract<ResultType>()) {
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
        /*
        void resolveNotification() override {
            mPromiseContract.first.setValue();
        }
        */

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
    std::map<RequestId, std::shared_ptr<OngoingRequestBase>> mOngoingRequests;
    std::recursive_mutex mOngoingRequestsMutex;
    std::chrono::milliseconds mRpcRequestTimeout;
    folly::CPUThreadPoolExecutor mExecutor;

public:
    explicit RpcCommunicatorClientApi(
        std::chrono::milliseconds rpcRequestTimeout)
        : mRpcRequestTimeout(rpcRequestTimeout), mExecutor(threadPoolSize) {}

    void setOutgoingMessageCallback(OutgoingMessageCallbackType callback) {
        mOutgoingMessageCallback = std::move(callback);
    }

    void onIncomingMessage(
        const RpcMessage& rpcMessage,
        const CallContextType& /* callContext */) {
        std::lock_guard lock(mOngoingRequestsMutex);

        const auto& header = rpcMessage.header();
        if (header.find("response") != header.end()) {
            SLogger::trace("onIncomingMessage() message is a response");
            if (mOngoingRequests.find(RequestId{rpcMessage.requestid()}) !=
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

    // To be called by auto-generated clients through RpcCommunicator

    template <typename ReturnType, typename RequestType>
    Task<ReturnType> request(
        const std::string& methodName,
        const RequestType& methodParam,
        const CallContextType& callContext,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        SLogger::trace("request(): methodName:", methodName);

        std::chrono::milliseconds timeoutValue = mRpcRequestTimeout;
        if (timeout.has_value()) {
            timeoutValue = timeout.value();
        }

        auto requestMessage =
            this->createRequestRpcMessage(methodName, methodParam);

        auto task = folly::coro::co_invoke(
            [requestMessage, callContext, timeoutValue, this]()
                -> folly::coro::Task<ReturnType> {
                auto callMakingTask = folly::coro::co_invoke(
                    [requestMessage,
                     callContext,
                     this]() -> folly::coro::Task<ReturnType> {
                        auto ongoingRequest = this->makeRpcRequest<ReturnType>(
                            requestMessage, callContext);
                        co_return co_await std::move(
                            ongoingRequest->getFuture());
                    });

                try {
                    co_return co_await folly::coro::timeout(
                        folly::coro::detachOnCancel(
                            std::move(callMakingTask).scheduleOn(&mExecutor)),
                        timeoutValue);
                } catch (const folly::FutureTimeout& e) {
                    SLogger::trace(
                        "request() caught folly::FutureTimeout", e.what());
                    std::lock_guard lock(mOngoingRequestsMutex);
                    mOngoingRequests.erase(
                        RequestId{requestMessage.requestid()});
                    throw RpcTimeout("request() timed out");
                } catch (...) {
                    SLogger::trace("request() caught other exception");
                    std::lock_guard lock(mOngoingRequestsMutex);
                    mOngoingRequests.erase(
                        RequestId{requestMessage.requestid()});
                    throw;
                }
            });

        return task;
    }

    // To be called by auto-generated clients through RpcCommunicator
    template <typename RequestType>
    Task<void> notify(
        const std::string_view notificationName,
        const RequestType& notificationParam,
        const CallContextType& callContext,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        SLogger::trace("notify() notificationName:", notificationName);

        std::chrono::milliseconds timeoutValue = mRpcRequestTimeout;
        if (timeout.has_value()) {
            timeoutValue = timeout.value();
        }

        SLogger::trace(
            "notify() creating request message, notificationName:",
            notificationName);
        auto requestMessage = this->createRequestRpcMessage(
            notificationName, notificationParam, true);
        auto&& promiseContract = folly::coro::makePromiseContract<void>();

        try {
            mExecutor.add(
                [requestMessage,
                 callContext,
                 promise = std::move(promiseContract.first),
                 outgoingMessageCallback =
                     mOutgoingMessageCallback]() mutable -> void {
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
                    .scheduleOn(&mExecutor),
                timeoutValue);
        } catch (const folly::FutureTimeout& e) {
            SLogger::trace("notify() timed out");
            throw RpcTimeout("notify() timed out");
        } catch (...) {
            SLogger::trace("notify() caught other exception");
            throw;
        }
    }

    [[nodiscard]] std::vector<RequestId>
    getOngoingRequestIdsFulfillingPredicate(
        const OngoingRequestPredicate& predicate) {
        std::scoped_lock lock(mOngoingRequestsMutex);
        std::vector<RequestId> ongoingRequestIds;
        for (const auto& ongoingRequest : mOngoingRequests) {
            if (ongoingRequest.second->fulfilsPredicate(predicate)) {
                ongoingRequestIds.push_back(ongoingRequest.first);
            }
        }
        return ongoingRequestIds;
    }

    void handleClientError(
        const RequestId& requestId, const RpcException& error) {
        std::lock_guard lock(mOngoingRequestsMutex);
        const auto& ongoingRequest = mOngoingRequests.find(requestId);

        if (ongoingRequest != mOngoingRequests.end()) {
            ongoingRequest->second->rejectRequest(error);
            mOngoingRequests.erase(requestId);
        }
    }

private:
    template <typename ReturnType>
    std::shared_ptr<OngoingRequest<ReturnType>> makeRpcRequest(
        const RpcMessage& requestMessage, const CallContextType& callContext) {
        auto ongoingRequest =
            std::make_shared<OngoingRequest<ReturnType>>(callContext);
        {
            std::lock_guard lock(mOngoingRequestsMutex);
            this->mOngoingRequests.emplace(
                RequestId{requestMessage.requestid()}, ongoingRequest);
        }
        if (mOutgoingMessageCallback) {
            try {
                mOutgoingMessageCallback(
                    requestMessage, requestMessage.requestid(), callContext);
            } catch (const std::exception& clientSideException) {
                std::lock_guard lock(mOngoingRequestsMutex);
                if (mOngoingRequests.find(
                        RequestId{requestMessage.requestid()}) !=
                    mOngoingRequests.end()) {
                    SLogger::debug(
                        "Error when calling outgoing message callback from client",
                        clientSideException.what());
                    RpcClientError error(
                        "Error when calling outgoing message callback from client",
                        clientSideException.what());

                    SLogger::debug("Old exception:", error.originalErrorInfo);

                    this->handleClientError(
                        RequestId{requestMessage.requestid()}, error);
                }
            }
        }
        return ongoingRequest;
    }

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
        ret.set_requestid(Uuid::v4());
        return ret;
    }

    void resolveOngoingRequest(const RpcMessage& response) {
        std::lock_guard lock(mOngoingRequestsMutex);
        auto& ongoingRequest =
            mOngoingRequests.at(RequestId{response.requestid()});
        ongoingRequest->resolveRequest(response);
        mOngoingRequests.erase(RequestId{response.requestid()});
    }

    void rejectOngoingRequest(const RpcMessage& response) {
        SLogger::trace("rejectOngoingRequest()", response.DebugString());
        std::lock_guard lock(mOngoingRequestsMutex);

        const auto& ongoingRequest =
            mOngoingRequests.at(RequestId{response.requestid()});

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
        mOngoingRequests.erase(RequestId{response.requestid()});
    }
};

} // namespace streamr::protorpc
#endif // STREAMR_PROTO_RPC_RPC_COMMUNICATOR_CLIENT_API_HPP