// Module streamr.protorpc.ServerRegistry
// CONSOLIDATED from the former header streamr-proto-rpc/ServerRegistry.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
//
// Async handlers (follow-up to Phase A4): alongside the synchronous
// std::function<Any(Any, Ctx)> handlers, methods may be registered as
// folly::coro::Task<Response>-returning coroutines via
// registerRpcMethodAsync. getAsyncHandler() returns a unified
// Task<Any>-returning handler for either kind (a sync handler is wrapped
// in a trivial `co_return syncFn(...)`), so the RPC-communicator server
// layer can await every handler without blocking the delivery thread.
// The synchronous handleRequest() / registerRpcMethod() path is kept
// unchanged for direct callers that still want an inline result.
module;
#include <new>

// std::coroutine_traits must be visible in every translation unit that
// defines OR instantiates a coroutine; it cannot arrive through an
// imported BMI.

#include <google/protobuf/any.pb.h>
#include <google/protobuf/empty.pb.h>
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"

export module streamr.protorpc.ServerRegistry;

import std;

import streamr.utils.CoroutineHelper;
import streamr.logger.SLogger;
import streamr.protorpc.Errors;
export namespace streamr::protorpc {

using Any = google::protobuf::Any;
using Empty = google::protobuf::Empty;
using RpcMessage = ::protorpc::RpcMessage;
using SLogger = streamr::logger::SLogger;
struct MethodOptions {
    std::size_t timeout = 0;
};

template <typename CallContextType>
class ServerRegistry {
private:
    using RpcMethodType =
        std::function<Any(Any request, CallContextType callContext)>;

    using RpcNotificationType =
        std::function<Empty(Any request, CallContextType callContext)>;

    // A coroutine handler that resolves the response Any without blocking
    // the calling thread. Parameters are taken by value so the coroutine
    // frame owns them across suspension points.
    using AsyncRpcMethodType = std::function<folly::coro::Task<Any>(
        Any request, CallContextType callContext)>;

    struct RegisteredMethod {
        RpcMethodType fn;
        MethodOptions options;
    };

    struct RegisteredAsyncMethod {
        AsyncRpcMethodType fn;
        MethodOptions options;
    };

    struct RegisteredNotification {
        RpcNotificationType fn;
        MethodOptions options;
    };

    std::map<std::string, RegisteredMethod> mMethods;
    std::map<std::string, RegisteredAsyncMethod> mAsyncMethods;
    std::map<std::string, RegisteredNotification> mNotifications;

    template <typename T>
    T getImplementation(
        const RpcMessage& rpcMessage, const std::map<std::string, T>& map) {
        const auto& header = rpcMessage.header();
        if (!header.contains("method")) {
            throw UnknownRpcMethod(
                "Header \"method\" missing from RPC message");
        }
        const auto& method = header.at("method");
        if (map.find(method) == map.end()) {
            throw UnknownRpcMethod("RPC Method " + method + " is not provided");
        }
        return map.at(method);
    }

public:
    template <typename TargetType>
    static void wrappedParseAny(TargetType& target, const Any& any) {
        try {
            any.UnpackTo(&target);
        } catch (...) {
            throw FailedToParse("Could not parse binary to JSON-object");
        }
    }

    Any handleRequest(
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        SLogger::trace(
            ("Server processing RPC call " + rpcMessage.requestid()));
        auto implementation = getImplementation(rpcMessage, mMethods);
        return implementation.fn(rpcMessage.body(), callContext);
    }

    // Returns a unified Task<Any>-returning handler for the method named in
    // `rpcMessage`, resolving to either a registered async handler or a
    // synchronous handler wrapped in a trivial coroutine. Returns
    // std::nullopt when the method is unknown, so the caller can build the
    // UNKNOWN_RPC_METHOD response itself (the lookup is cheap and happens on
    // the delivery thread while `this` is guaranteed alive; the returned
    // handler is self-contained and safe to run on another thread).
    std::optional<AsyncRpcMethodType> getAsyncHandler(
        const RpcMessage& rpcMessage) {
        const auto& header = rpcMessage.header();
        if (!header.contains("method")) {
            return std::nullopt;
        }
        const auto& method = header.at("method");
        if (const auto it = mAsyncMethods.find(method);
            it != mAsyncMethods.end()) {
            return it->second.fn;
        }
        if (const auto it = mMethods.find(method); it != mMethods.end()) {
            RpcMethodType syncFn = it->second.fn;
            return AsyncRpcMethodType(
                [syncFn](
                    Any request,
                    CallContextType callContext) -> folly::coro::Task<Any> {
                    co_return syncFn(
                        std::move(request), std::move(callContext));
                });
        }
        return std::nullopt;
    }

    Empty handleNotification(
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        SLogger::trace(
            ("Server processing RPC notification " + rpcMessage.requestid()));
        auto implementation = getImplementation(rpcMessage, mNotifications);
        return implementation.fn(rpcMessage.body(), callContext);
    }

    template <typename RequestType, typename ReturnType, typename F>
    void registerRpcMethod(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        RegisteredMethod method = {
            .fn = [fn](const Any& data, const CallContextType& callContext)
                -> Any {
                RequestType request;
                ServerRegistry::wrappedParseAny(request, data);
                auto response = fn(request, callContext);
                Any responseAny;
                responseAny.PackFrom(response);
                return responseAny;
            },
            .options = options};
        mMethods[name] = method;
    }

    // Register a coroutine handler. `fn` takes (RequestType, CallContext)
    // and returns folly::coro::Task<ReturnType>; the wrapper parses the
    // request, awaits the handler and packs the response. The handler is
    // awaited by the server layer, so it may co_await a worker executor
    // without blocking the delivery thread.
    template <typename RequestType, typename ReturnType, typename F>
    void registerRpcMethodAsync(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        RegisteredAsyncMethod method = {
            .fn = [fn](Any data, CallContextType callContext)
                -> folly::coro::Task<Any> {
                RequestType request;
                ServerRegistry::wrappedParseAny(request, data);
                ReturnType response = co_await fn(request, callContext);
                Any responseAny;
                responseAny.PackFrom(response);
                co_return responseAny;
            },
            .options = options};
        mAsyncMethods[name] = method;
    }

    template <typename RequestType, typename F>
    void registerRpcNotification(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        RegisteredNotification notification = {
            .fn = [fn](const Any& data, const CallContextType& callContext)
                -> Empty {
                RequestType request;
                ServerRegistry::wrappedParseAny(request, data);
                fn(request, callContext);
                return {};
            },
            .options = options};
        mNotifications[name] = notification;
    }
};

} // namespace streamr::protorpc
