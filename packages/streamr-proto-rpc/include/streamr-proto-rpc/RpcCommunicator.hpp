#ifndef STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP
#define STREAMR_PROTO_RPC_RPC_COMMUNICATOR_HPP

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

#include "ProtoCallContext.hpp"
#include "ProtoRpc.pb.h"
#include "RpcCommunicatorClientApi.hpp"
#include "RpcCommunicatorServerApi.hpp"
#include "ServerRegistry.hpp"

#include "streamr-logger/SLogger.hpp"

namespace streamr::protorpc {

using google::protobuf::Any;
using streamr::logger::SLogger;
using RpcMessage = ::protorpc::RpcMessage;
using RpcErrorType = ::protorpc::RpcErrorType;
using folly::coro::Task;
using OutgoingMessageCallbackType =
    RpcCommunicatorClientApi::OutgoingMessageCallbackType;

constexpr size_t defaultRpcRequestTimeout = 5000;

// NOLINTBEGIN
enum class StatusCode { OK, STOPPED, DEADLINE_EXCEEDED, SERVER_ERROR };
// NOLINTEND

struct RpcCommunicatorConfig {
    size_t rpcRequestTimeout;
};
class RpcCommunicator {
private:
    RpcCommunicatorClientApi mRpcCommunicatorClientApi;
    RpcCommunicatorServerApi mRpcCommunicatorServerApi;

public:
    explicit RpcCommunicator(
        std::optional<RpcCommunicatorConfig> config = std::nullopt)
        : mRpcCommunicatorClientApi(
              config.has_value() ? config.value().rpcRequestTimeout
                                 : defaultRpcRequestTimeout) {}

    // Messaging API

    void handleIncomingMessage(
        const RpcMessage& message, const ProtoCallContext& callContext) {
        this->onIncomingMessage(message, callContext);
    }

    template <typename F>
        requires std::is_assignable_v<OutgoingMessageCallbackType, F>
    void setOutgoingMessageCallback(F&& callback) {
        mRpcCommunicatorClientApi.setOutgoingMessageCallback(
            std::forward<F>(callback));
        mRpcCommunicatorServerApi.setOutgoingMessageCallback(
            std::forward<F>(callback));
    }

    // Client-side API
    template <typename ReturnType, typename RequestType>
    Task<ReturnType> callRemote(
        const std::string& methodName,
        const RequestType& methodParam,
        const ProtoCallContext& callContext) {
        return mRpcCommunicatorClientApi.callRemote<ReturnType, RequestType>(
            methodName, methodParam, callContext);
    }

    template <typename RequestType>
    Task<void> notifyRemote(
        const std::string_view methodName,
        const RequestType& methodParam,
        const ProtoCallContext& callContext) {
        return mRpcCommunicatorClientApi.notifyRemote<RequestType>(
            methodName, methodParam, callContext);
    }

    // Server-side API
    template <typename RequestType, typename ReturnType, typename F>
        requires std::is_assignable_v<
            std::function<ReturnType(RequestType, ProtoCallContext)>,
            F>
    void registerRpcMethod(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mRpcCommunicatorServerApi.registerRpcMethod<RequestType, ReturnType, F>(
            name, fn, options);
    }

    template <typename RequestType, typename F>
        requires std::is_assignable_v<
            std::function<void(RequestType, ProtoCallContext)>,
            F>
    void registerRpcNotification(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mRpcCommunicatorServerApi.registerRpcNotification<RequestType, F>(
            name, fn, options);
    }

private:
    void onIncomingMessage(
        const RpcMessage& rpcMessage, const ProtoCallContext& callContext) {
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