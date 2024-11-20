#ifndef STREAMR_PROTO_RPC_ERRORS_HPP
#define STREAMR_PROTO_RPC_ERRORS_HPP

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <magic_enum.hpp>

namespace streamr::protorpc {

// NOLINTBEGIN
enum class ErrorCode {
    RPC_TIMEOUT,
    RPC_REQUEST,
    RPC_SERVER_ERROR,
    RPC_CLIENT_ERROR,
    NOT_IMPLEMENTED,
    UNKNOWN_RPC_METHOD,
    FAILED_TO_PARSE,
    FAILED_TO_SERIALIZE
};
// NOLINTEND

class Err : public std::runtime_error {
public:
    ErrorCode code; // NOLINT
    std::string message; // NOLINT
    std::optional<std::string> originalErrorInfo; // NOLINT

    Err(ErrorCode code,
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : std::runtime_error(message),
          code(code),
          message(message),
          originalErrorInfo(originalErrorInfo) {
        createStringRepresentation();
    }

    [[nodiscard]] virtual std::string toString() const {
        return stringRepresentation;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return stringRepresentation.c_str();
    }

private:
    std::string stringRepresentation; // NOLINT

    void createStringRepresentation() {
        stringRepresentation =
            "code: " + std::string(magic_enum::enum_name(code));
        stringRepresentation += ", message: " + message;
        if (originalErrorInfo.has_value()) {
            stringRepresentation +=
                ", originalErrorInfo: " + originalErrorInfo.value();
        }
    }
};

struct RpcTimeout : public Err {
    explicit RpcTimeout(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::RPC_TIMEOUT, message, originalErrorInfo) {}
};

struct FailedToParse : public Err {
    explicit FailedToParse(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::FAILED_TO_PARSE, message, originalErrorInfo) {}
};

struct FailedToSerialize : public Err {
    explicit FailedToSerialize(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::FAILED_TO_SERIALIZE, message, originalErrorInfo) {}
};

struct UnknownRpcMethod : public Err {
    explicit UnknownRpcMethod(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::UNKNOWN_RPC_METHOD, message, originalErrorInfo) {}
};

struct RpcRequestError : public Err {
    explicit RpcRequestError(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::RPC_REQUEST, message, originalErrorInfo) {}
};

struct RpcClientError : public Err {
    explicit RpcClientError(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::RPC_CLIENT_ERROR, message, originalErrorInfo) {}
};

class RpcServerError : public Err {
public:
    std::string errorClassName; // NOLINT
    std::string errorCode; // NOLINT

    RpcServerError(
        const std::string& errorMessage, // NOLINT
        const std::string& errorClassName, // NOLINT
        const std::string& errorCode) // NOLINT
        : Err(ErrorCode::RPC_SERVER_ERROR, errorMessage),
          errorClassName(errorClassName),
          errorCode(errorCode) {
        createStringRepresentation();
    }

    [[nodiscard]] std::string toString() const override {
        return stringRepresentation;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return stringRepresentation.c_str();
    }

private:
    std::string stringRepresentation; // NOLINT

    void createStringRepresentation() {
        stringRepresentation = Err::toString();
        stringRepresentation += ", errorClassName: " + errorClassName;
        stringRepresentation += ", errorCode: " + errorCode;
    }
};

using RpcException = std::variant<
    RpcTimeout,
    RpcRequestError,
    RpcServerError,
    RpcClientError,
    UnknownRpcMethod,
    FailedToParse,
    FailedToSerialize>;

/*
export class RpcTimeout extends Err { constructor(message?: string,
originalError?: Error | string) { super(ErrorCode.RPC_TIMEOUT, message,
originalError) } } export class RpcRequest extends Err { constructor(message?:
string, originalError?: Error | string) { super(ErrorCode.RPC_REQUEST, message,
originalError) } } export class RpcServerError extends Err { public
errorClassName?: string public errorCode?: string

    constructor(errorMessage?: string, errorClassName?: string, errorCode?:
string) { super(ErrorCode.RPC_SERVER_ERROR, errorMessage) this.errorClassName =
errorClassName this.errorCode = errorCode
    }
}
export class RpcClientError extends Err { constructor(message?: string,
originalError?: Error | string) { super(ErrorCode.RPC_CLIENT_ERROR, message,
originalError) } } export class NotImplemented extends Err {
constructor(message?: string, originalError?: Error | string) {
super(ErrorCode.NOT_IMPLEMENTED, message, originalError) } } export class
UnknownRpcMethod extends Err { constructor(message?: string, originalError?:
Error | string) { super(ErrorCode.UNKNOWN_RPC_METHOD, message, originalError) }
} export class FailedToParse extends Err { constructor(message?: string,
originalError?: Error | string) { super(ErrorCode.FAILED_TO_PARSE, message,
originalError) } } export class FailedToSerialize extends Err {
constructor(message?: string, originalError?: Error | string) {
super(ErrorCode.FAILED_TO_SERIALIZE, message, originalError) } }

*/

} // namespace streamr::protorpc

#endif // STREAMR_PROTO_RPC_ERRORS_HPP
