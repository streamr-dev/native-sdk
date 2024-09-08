#ifndef STREAMR_PROTO_RPC_SERVER_REGISTRY_HPP
#define STREAMR_PROTO_RPC_SERVER_REGISTRY_HPP

#include <google/protobuf/any.pb.h>
#include <google/protobuf/empty.pb.h>
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"
#include "streamr-logger/SLogger.hpp"
#include "streamr-proto-rpc/Errors.hpp"

namespace streamr::protorpc {

using Any = google::protobuf::Any;
using Empty = google::protobuf::Empty;
using RpcMessage = ::protorpc::RpcMessage;
using SLogger = streamr::logger::SLogger;
struct MethodOptions {
    size_t timeout = 0;
};

template <typename CallContextType>
class ServerRegistry {

private:
    using RpcMethodType =
        std::function<Any(Any request, CallContextType callContext)>;

    using RpcNotificationType =
        std::function<Empty(Any request, CallContextType callContext)>;
    struct RegisteredMethod {
        RpcMethodType fn;
        MethodOptions options;
    };

    struct RegisteredNotification {
        RpcNotificationType fn;
        MethodOptions options;
    };

    std::map<std::string, RegisteredMethod> mMethods;
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

#endif // STREAMR_PROTO_RPC_SERVER_REGISTRY_HPP