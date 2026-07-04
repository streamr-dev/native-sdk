// Module streamr.dht.Errors
// CONSOLIDATED from the former header streamr-dht/helpers/Errors.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <magic_enum/magic_enum.hpp>

export module streamr.dht.Errors;
export namespace streamr::dht::helpers {

// NOLINTBEGIN
enum class ErrorCode {
    STARTING_WEBSOCKET_SERVER_FAILED,
    COULD_NOT_START,
    CANNOT_CONNECT_TO_SELF,
    SEND_FAILED
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

struct WebsocketServerStartError : public Err {
    explicit WebsocketServerStartError(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::STARTING_WEBSOCKET_SERVER_FAILED,
              message,
              originalErrorInfo) {}
};

struct CouldNotStart : public Err {
    explicit CouldNotStart(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::COULD_NOT_START, message, originalErrorInfo) {}
};

struct CannotConnectToSelf : public Err {
    explicit CannotConnectToSelf(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::CANNOT_CONNECT_TO_SELF, message, originalErrorInfo) {}
};

struct SendFailed : public Err {
    explicit SendFailed(
        const std::string& message,
        const std::optional<std::string>& originalErrorInfo = std::nullopt)
        : Err(ErrorCode::SEND_FAILED, message, originalErrorInfo) {}
};

using DhtException = std::variant<
    WebsocketServerStartError,
    CouldNotStart,
    CannotConnectToSelf,
    SendFailed>;

} // namespace streamr::dht::helpers
