// Module streamr.protorpc.RpcCommunicatorClientApi
// CONSOLIDATED from the former header
// streamr-proto-rpc/RpcCommunicatorClientApi.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <atomic>
#include <exception>
#include <map>
#include <mutex>
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"

export module streamr.protorpc.RpcCommunicatorClientApi;

import streamr.utils.CoroutineHelper;
import streamr.utils.ExecutorHelper;
import streamr.utils.SharedExecutors;
import streamr.logger.SLogger;
import streamr.utils.Branded;
import streamr.utils.Uuid;
import streamr.protorpc.Errors;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using folly::coro::Task;
using google::protobuf::Any;
using streamr::logger::SLogger;
using streamr::utils::Branded;
using streamr::utils::Uuid;
export namespace streamr::protorpc {

using RpcMessage = ::protorpc::RpcMessage;
using RpcErrorType = ::protorpc::RpcErrorType;

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
    // Owns every detached/abandonable coroutine that touches `this`
    // (request/notify below): the scope is drained in the destructor, which
    // is the per-instance teardown guarantee the former private thread
    // pool's destructor-join used to give (a pool per communicator did not
    // scale to hundreds of nodes in one process, see
    // streamr.utils.SharedExecutors).
    folly::coro::CancellableAsyncScope mScope;
    // drainAsyncTasks() must run the cancel-and-join exactly once (folly
    // AsyncScope forbids a second join).
    std::atomic<bool> mDrained = false;

public:
    explicit RpcCommunicatorClientApi(
        std::chrono::milliseconds rpcRequestTimeout)
        : mRpcRequestTimeout(rpcRequestTimeout) {}

    // Drains in-flight request/notify coroutines. Idempotent. Owners whose
    // straggler tasks reach through members that die before this subobject
    // (RoutingRpcCommunicator's sendFn, ConnectionManager's endpoints) must
    // call this BEFORE those members are destroyed — the destructor drain
    // below is only a backstop that runs after the owning object's members
    // are already gone. Must run on a thread other than the shared pool
    // worker currently executing one of this instance's tasks (it always
    // does: the communicator is owned and destroyed by the transport/main
    // thread, never from inside its own request path).
    void drainAsyncTasks() noexcept {
        if (mDrained.exchange(true)) {
            return;
        }
        try {
            streamr::utils::blockingWait(mScope.cancelAndJoinAsync());
        } catch (...) { // NOLINT(bugprone-empty-catch) must not throw
        }
    }

    // Number of request/notify coroutines still owned by the scope. When
    // zero, drainAsyncTasks()/the destructor complete without needing any
    // pool thread — the probe owners use to decide whether a retired
    // communicator can be destroyed from a worker thread.
    [[nodiscard]] std::size_t pendingTaskCount() const noexcept {
        return mScope.remaining();
    }

    ~RpcCommunicatorClientApi() { this->drainAsyncTasks(); }

    RpcCommunicatorClientApi(const RpcCommunicatorClientApi&) = delete;
    RpcCommunicatorClientApi& operator=(const RpcCommunicatorClientApi&) =
        delete;
    RpcCommunicatorClientApi(RpcCommunicatorClientApi&&) = delete;
    RpcCommunicatorClientApi& operator=(RpcCommunicatorClientApi&&) = delete;

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
        if (mDrained) {
            // A drained scope must not be add()ed to (folly forbids
            // add-after-join); a request after drainAsyncTasks() is a
            // stop()-ordering bug in the caller — fail it cleanly.
            throw RpcClientError(
                "request() called on a drained RpcCommunicator: " + methodName);
        }

        std::chrono::milliseconds timeoutValue = mRpcRequestTimeout;
        if (timeout.has_value()) {
            timeoutValue = timeout.value();
        }

        auto requestMessage =
            this->createRequestRpcMessage(methodName, methodParam);

        auto task = folly::coro::co_invoke(
            [requestMessage, callContext, timeoutValue, this]()
                -> folly::coro::Task<ReturnType> {
                // The `this`-touching work runs as an mScope task on the
                // shared pool; the caller awaits only the contract future,
                // which holds no `this`, so an abandoned caller cannot leave
                // a dangling reference. The TIMEOUT lives INSIDE the scope
                // task, wrapped around the request-future await: a timed-out
                // request erases its map entry (below), after which nothing
                // could ever resolve that future — an unbounded await here
                // would then hold the scope task forever and deadlock the
                // destructor's drain (seen as a 300 s teardown hang on the
                // linux-arm64 CI runner when gracefullyDisconnect left a
                // pending request behind). Bounded by the timeout, every
                // scope task settles.
                auto&& contract =
                    folly::coro::makePromiseContract<ReturnType>();
                mScope.add(
                    streamr::utils::co_withExecutor(
                        &streamr::utils::SharedExecutors::worker(),
                        folly::coro::co_invoke(
                            [requestMessage,
                             callContext,
                             timeoutValue,
                             this,
                             promise = std::move(contract.first)]() mutable
                                -> folly::coro::Task<void> {
                                try {
                                    auto ongoingRequest =
                                        this->makeRpcRequest<ReturnType>(
                                            requestMessage, callContext);
                                    promise.setValue(
                                        co_await folly::coro::timeout(
                                            std::move(
                                                ongoingRequest->getFuture()),
                                            timeoutValue));
                                } catch (...) {
                                    promise.setException(
                                        folly::exception_wrapper(
                                            std::current_exception()));
                                }
                            })));

                try {
                    co_return co_await folly::coro::detachOnCancel(
                        std::move(contract.second));
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
        if (mDrained) {
            // See request(): no adds after the scope drain.
            throw RpcClientError(
                "notify() called on a drained RpcCommunicator: " +
                std::string(notificationName));
        }

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
            // The send callback may reach into the transport (it captures
            // the communicator owner), so it runs as an mScope task — the
            // scope drain in the destructor replaces the former private
            // executor's join.
            mScope.add(
                streamr::utils::co_withExecutor(
                    &streamr::utils::SharedExecutors::worker(),
                    folly::coro::co_invoke(
                        [requestMessage,
                         callContext,
                         promise = std::move(promiseContract.first),
                         outgoingMessageCallback =
                             mOutgoingMessageCallback]() mutable
                            -> folly::coro::Task<void> {
                            try {
                                outgoingMessageCallback(
                                    requestMessage,
                                    requestMessage.requestid(),
                                    callContext);
                                promise.setValue();
                            } catch (
                                const std::exception& clientSideException) {
                                SLogger::debug(
                                    "Error when calling outgoing message callback from client for sending notification",
                                    clientSideException.what());
                                promise.setException(RpcClientError(
                                    "Error when calling outgoing message callback from client for sending notification",
                                    clientSideException.what()));
                            }
                            co_return;
                        })));
            co_return co_await folly::coro::timeout(
                folly::coro::detachOnCancel(std::move(promiseContract.second)),
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
