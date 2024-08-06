#ifndef STREAMR_PROTO_RPC_SERVER_REGISTRY_HPP
#define STREAMR_PROTO_RPC_SERVER_REGISTRY_HPP

#include <type_traits>
#include <google/protobuf/any.pb.h>
#include <google/protobuf/empty.pb.h>
#include "ProtoRpc.pb.h"
#include "streamr-logger/SLogger.hpp"
#include "streamr-proto-rpc/Errors.hpp"
#include "streamr-proto-rpc/ProtoCallContext.hpp"

namespace streamr::protorpc {

using Any = google::protobuf::Any;
using Empty = google::protobuf::Empty;

using RpcMessage = ::protorpc::RpcMessage;
using SLogger = streamr::logger::SLogger;

using RpcMethodType =
    std::function<Any(Any request, ProtoCallContext callContext)>;

using RpcNotificationType =
    std::function<Empty(Any request, ProtoCallContext callContext)>;

// using ParseFunctionType = std::function<void(Any request)>;

template <typename T>
concept AssignableToRpcMethod = std::is_assignable_v<RpcMethodType, T>;

template <typename T>
concept AssignableToRpcNotification =
    std::is_assignable_v<RpcNotificationType, T>;

// template <typename T>
// concept AssignableToParseFunction = std::is_assignable<ParseFunctionType,
// T>::value;

struct MethodOptions {
    size_t timeout = 0;
};

struct RegisteredMethod {
    RpcMethodType fn;
    MethodOptions options;
};

struct RegisteredNotification {
    RpcNotificationType fn;
    MethodOptions options;
};

class ServerRegistry {
private:
    std::map<std::string, RegisteredMethod> mMethods;
    std::map<std::string, RegisteredNotification> mNotifications;

    template <typename T>
    T getImplementation(
        const RpcMessage& rpcMessage, const std::map<std::string, T>& map) {
        // std::cout << "getImplementation()" << rpcMessage.header() <<
        // std::endl;
        SLogger::info("header length", rpcMessage.header().size());
        if (!rpcMessage.header().contains("method")) {
            throw UnknownRpcMethod(
                "Header \"method\" missing from RPC message");
        }

        if (map.find(rpcMessage.header().at("method")) == map.end()) {
            throw UnknownRpcMethod(
                "RPC Method " + rpcMessage.header().at("method") +
                " is not provided");
        }

        return map.at(rpcMessage.header().at("method"));
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

    /*
    public async handleRequest(rpcMessage: RpcMessage, callContext?:
    ProtoCallContext): Promise<Any> {

        logger.trace(`Server processing RPC call ${rpcMessage.requestId}`)

        const implementation = this.getImplementation(rpcMessage, this.methods)
        const timeout = implementation.options.timeout!
        return await promiseTimeout(timeout, implementation.fn(rpcMessage.body!,
    callContext ? callContext : new ProtoCallContext()))
    }

    */

    Any handleRequest(
        const RpcMessage& rpcMessage, const ProtoCallContext& callContext) {
        SLogger::trace(
            ("Server processing RPC call " + rpcMessage.requestid()).c_str());
        auto implementation = getImplementation(rpcMessage, mMethods);
        return implementation.fn(rpcMessage.body(), callContext);
    }

    Empty handleNotification(
        const RpcMessage& rpcMessage, const ProtoCallContext& callContext) {
        auto implementation = getImplementation(rpcMessage, mNotifications);
        return implementation.fn(rpcMessage.body(), callContext);
    }

    /*
    public registerRpcMethod<RequestClass extends IMessageType<RequestType>,
        ReturnClass extends IMessageType<ReturnType>,
        RequestType extends object,
        ReturnType extends object>(
        requestClass: RequestClass,
        returnClass: ReturnClass,
        name: string,
        fn: (rq: RequestType, _context: ProtoCallContext) =>
    Promise<ReturnType>, opts: MethodOptions = {}
    ): void {
        const options = parseOptions(opts)
        const method = {
            fn: async (data: Any, callContext: ProtoCallContext) => {
                const request = parseWrapper(() => Any.unpack(data,
    requestClass)) const response = await fn(request, callContext) return
    Any.pack(response, returnClass)
            },
            options
        }
        this.methods.set(name, method)
    }
    */

    template <typename RequestType, typename ReturnType, typename F>
    void registerRpcMethod(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        RegisteredMethod method = {
            .fn = [&fn](const Any& data, const ProtoCallContext& callContext)
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
            .fn = [&fn](const Any& data, const ProtoCallContext& callContext)
                -> Empty {
                RequestType request;
                ServerRegistry::wrappedParseAny(request, data);
                fn(request, callContext);
                return Empty();
            },
            .options = options};
        mNotifications[name] = notification;
    }
};

} // namespace streamr::protorpc

#endif // STREAMR_PROTO_RPC_SERVER_REGISTRY_HPP