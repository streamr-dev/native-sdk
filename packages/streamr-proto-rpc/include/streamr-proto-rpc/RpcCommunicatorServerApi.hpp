#ifndef STREAMR_PROTO_RPC_RPC_COMMUNICATOR_SERVER_API_HPP
#define STREAMR_PROTO_RPC_RPC_COMMUNICATOR_SERVER_API_HPP

#include <exception>
#include "Errors.hpp"
#include "ServerRegistry.hpp"

namespace streamr::protorpc {
using RpcMessage = ::protorpc::RpcMessage;
using RpcErrorType = ::protorpc::RpcErrorType;

template <typename CallContextType>
class RpcCommunicatorServerApi {
public:
    using OutgoingMessageCallbackType = std::function<void(
        RpcMessage,
        std::string /*requestId*/,
        std::optional<
            std::function<void(std::exception_ptr)>> /*errorCallback*/,
        CallContextType)>;

private:
    ServerRegistry<CallContextType> mServerRegistry;
    OutgoingMessageCallbackType mOutgoingMessageCallback;

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
        SLogger::trace("createResponseRpcMessage()");
        RpcMessage ret;

        if (params.body.has_value()) {
            SLogger::trace(
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

    void handleRequest(
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        RpcMessage response;
        try {
            SLogger::trace("handleRequest()");
            auto bytes = mServerRegistry.handleRequest(rpcMessage, callContext);
            response = createResponseRpcMessage(
                {.request = rpcMessage, .body = bytes});
        } catch (const Err& err) {
            SLogger::debug("handleRequest() exception ", err.what());
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
            SLogger::trace(
                "handleRequest() creating response message for error");
            response = createResponseRpcMessage(errorParams);
        } catch (const std::exception& err) {
            SLogger::debug(
                "Non-RpcCommunicator error when handling request", err.what());
            RpcResponseParams errorParams = {.request = rpcMessage};
            errorParams.errorType = RpcErrorType::SERVER_ERROR;
            errorParams.errorClassName = typeid(err).name();
            errorParams.errorCode =
                magic_enum::enum_name(ErrorCode::RPC_SERVER_ERROR);
            errorParams.errorMessage = err.what();
            response = createResponseRpcMessage(errorParams);
        }

        if (mOutgoingMessageCallback) {
            try {
                mOutgoingMessageCallback(
                    response, response.requestid(), std::nullopt, callContext);
            } catch (const std::exception& clientSideException) {
                SLogger::debug(
                    "error when calling outgoing message callback from server",
                    clientSideException.what());
            }
        }
    }

    void handleNotification(
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        try {
            mServerRegistry.handleNotification(rpcMessage, callContext);
        } catch (const std::exception& err) {
            SLogger::debug("error", err.what());
        }
    }

public:
    void onIncomingMessage(
        const RpcMessage& rpcMessage, const CallContextType& callContext) {
        const auto& header = rpcMessage.header();

        if (header.find("request") != header.end() &&
            header.find("method") != header.end()) {
            SLogger::trace("onIncomingMessage() message is a request");
            if (header.find("notification") != header.end()) {
                SLogger::trace(
                    "onIncomingMessage() calling handleNotification()");
                this->handleNotification(rpcMessage, callContext);
                SLogger::trace(
                    "onIncomingMessage() handleNotification() called");

            } else {
                SLogger::trace("onIncomingMessage() calling handleRequest()");
                this->handleRequest(rpcMessage, callContext);
            }
        } else {
            SLogger::error(
                "onIncomingMessage() a message that is not a request was sent to RpcCommunicatorServerApi");
        }
    }

    void setOutgoingMessageCallback(OutgoingMessageCallbackType callback) {
        mOutgoingMessageCallback = std::move(callback);
    }

    template <typename RequestType, typename ReturnType, typename F>
        requires std::is_assignable_v<
            std::function<ReturnType(RequestType, CallContextType)>,
            F>
    void registerRpcMethod(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mServerRegistry.template registerRpcMethod<RequestType, ReturnType, F>(
            name, fn, options);
    }

    template <typename RequestType, typename F>
        requires std::is_assignable_v<
            std::function<void(RequestType, CallContextType)>,
            F>
    void registerRpcNotification(
        const std::string& name, const F& fn, MethodOptions options = {}) {
        mServerRegistry.template registerRpcNotification<RequestType, F>(
            name, fn, options);
    }
};

} // namespace streamr::protorpc

#endif // STREAMR_PROTO_RPC_RPC_COMMUNICATOR_SERVER_API_HPP