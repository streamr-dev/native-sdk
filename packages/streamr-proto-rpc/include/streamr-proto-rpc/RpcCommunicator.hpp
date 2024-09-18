#ifndef STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP
#define STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP

#include <chrono>
#include <optional>
#include <google/protobuf/any.pb.h>
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
#include "RpcCommunicatorClientApi.hpp"
#include "RpcCommunicatorServerApi.hpp"
#include "ServerRegistry.hpp"
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"

#include "streamr-logger/SLogger.hpp"

namespace streamr::protorpc {

using namespace std::chrono_literals;
using google::protobuf::Any;
using streamr::logger::SLogger;
using RpcMessage = ::protorpc::RpcMessage;
using RpcErrorType = ::protorpc::RpcErrorType;
using folly::coro::Task;

constexpr std::chrono::milliseconds defaultRpcRequestTimeout = 5000ms;

// NOLINTBEGIN
enum class StatusCode { OK, STOPPED, DEADLINE_EXCEEDED, SERVER_ERROR };
// NOLINTEND

struct RpcCommunicatorOptions {
    std::chrono::milliseconds rpcRequestTimeout;
};

template <typename CallContextType>
class RpcCommunicator {
private:
    RpcCommunicatorClientApi<CallContextType> mRpcCommunicatorClientApi;
    RpcCommunicatorServerApi<CallContextType> mRpcCommunicatorServerApi;

public:
    using OutgoingMessageCallbackType =
        RpcCommunicatorClientApi<CallContextType>::OutgoingMessageCallbackType;
    explicit RpcCommunicator(
        std::optional<RpcCommunicatorOptions> options = std::nullopt)
        : mRpcCommunicatorClientApi(
              options.has_value() ? options.value().rpcRequestTimeout
                                  : defaultRpcRequestTimeout) {}

    // Messaging API

    /**
     * @brief Handle a message incoming from the network
     *
     * @param message message received from the network
     * @param callContext call context such as routing information
     */

    void handleIncomingMessage(
        const RpcMessage& message, const CallContextType& callContext) {
        this->onIncomingMessage(message, callContext);
    }

    /**
     * @brief Set a callback for sending outgoing messages to the network
     *
     * @param callback callback to be called when a message is ready to be sent.
     * The callback may throw exeptions which will be forwarded through the
     * RpcCommunicator
     *
     */

    void setOutgoingMessageCallback(
        const OutgoingMessageCallbackType& callback) {
        mRpcCommunicatorClientApi.setOutgoingMessageCallback(callback);
        mRpcCommunicatorServerApi.setOutgoingMessageCallback(callback);
    }

    // Client-side API

    /**
     * @brief [A method to be called by auto-generated clients] Make a RPC
     * request to a remote method
     *
     * @param methodName name of the method to be called
     * @param methodParam parameter to be passed to the method
     * @param callContext call context such as routing information
     * @return Task<ReturnType>
     */

    template <typename ReturnType, typename RequestType>
    Task<ReturnType> request(
        const std::string& methodName,
        const RequestType& methodParam,
        const CallContextType& callContext,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return mRpcCommunicatorClientApi
            .template request<ReturnType, RequestType>(
                methodName, methodParam, callContext, timeout);
    }

    /**
     * @brief [A method to be called by auto-generated clients] Make a
     * remote RPC notification
     *
     * @param notificationName name of the notification to be called
     * @param notificationParam parameter to be passed to the notification
     * @param callContext call context such as routing information
     * @return Task<void>
     */

    template <typename RequestType>
    Task<void> notify(
        const std::string_view notificationName,
        const RequestType& notificationParam,
        const CallContextType& callContext,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt) {
        return mRpcCommunicatorClientApi.template notify<RequestType>(
            notificationName, notificationParam, callContext, timeout);
    }

    // Server-side API

    /**
     * @brief Register a method to be called by remote clients
     *
     * @param name name of the method
     * @param fn function to be registered
     * @param options options for the method
     */

    template <typename RequestType, typename ReturnType, typename F>
        requires std::is_assignable_v<
            std::function<ReturnType(RequestType, CallContextType)>,
            F>
    void registerRpcMethod(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mRpcCommunicatorServerApi
            .template registerRpcMethod<RequestType, ReturnType, F>(
                name, fn, options);
    }

    /**
     * @brief Register a notification to be called by remote clients
     *
     * @param name name of the notification
     * @param fn function to be registered
     * @param options options for the notification
     */

    template <typename RequestType, typename F>
        requires std::is_assignable_v<
            std::function<void(RequestType, CallContextType)>,
            F>
    void registerRpcNotification(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mRpcCommunicatorServerApi
            .template registerRpcNotification<RequestType, F>(
                name, fn, options);
    }

private:
    void onIncomingMessage(
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        SLogger::trace("onIncomingMessage", rpcMessage.DebugString());

        const auto& header = rpcMessage.header();

        SLogger::trace("onIncomingMessage() requestId", rpcMessage.requestid());

        if (header.find("response") != header.end()) {
            mRpcCommunicatorClientApi.onIncomingMessage(
                rpcMessage, callContext);
        } else if (
            header.find("request") != header.end() &&
            header.find("method") != header.end()) {
            mRpcCommunicatorServerApi.onIncomingMessage(
                rpcMessage, callContext);
        } else {
            SLogger::debug(
                "onIncomingMessage() message is not a valid request or response");
        }
    }
    // Client-side functions
};

} // namespace streamr::protorpc

#endif // STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP