#ifndef STREAMR_DHT_UTILS_ERRORS_HPP
#define STREAMR_DHT_UTILS_ERRORS_HPP

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <magic_enum.hpp>

namespace streamr::dht::utils {

// NOLINTBEGIN
enum class ErrorCode { STARTING_WEBSOCKET_SERVER_FAILED };
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

struct WebsocketServerStartError : public Err {
    explicit WebsocketServerStartError(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::STARTING_WEBSOCKET_SERVER_FAILED,
              message,
              originalErrorInfo) {}
};

using DhtException = std::variant<WebsocketServerStartError>;

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

} // namespace streamr::dht::utils

#endif // STREAMR_PROTO_RPC_ERRORS_HPP
