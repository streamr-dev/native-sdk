#ifndef STREAMER_LOGGER_LOGGER_HPP
#define STREAMER_LOGGER_LOGGER_HPP

#include <source_location>
#include <folly/logging/LogConfig.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include <folly/logging/LogWriter.h>
#include <folly/logging/LoggerDB.h>
#include <folly/logging/xlog.h>
#include "StreamrHandlerFactory.hpp"
#include "StreamrWriterFactory.hpp"
#include "streamr-json/toJson.hpp"
#include "streamr-json/toString.hpp"

namespace streamr::logger {

namespace detail {

static constexpr std::string_view envLogLevelName = "LOG_LEVEL";
static constexpr std::string_view logLevelTraceName = "trace";
static constexpr std::string_view logLevelDebugName = "debug";
static constexpr std::string_view logLevelInfoName = "info";
static constexpr std::string_view logLevelWarnName = "warn";
static constexpr std::string_view logLevelErrorName = "error";
static constexpr std::string_view logLevelFatalName = "fatal";

enum class StreamrLogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };

// This function can be used at compile time
constexpr folly::LogLevel getFollyLogLevel(StreamrLogLevel streamrLogLevel) {
    if (streamrLogLevel == StreamrLogLevel::TRACE) {
        return folly::LogLevel::DBG;
    }
    if (streamrLogLevel == StreamrLogLevel::DEBUG) {
        return folly::LogLevel::DBG0;
    }
    if (streamrLogLevel == StreamrLogLevel::INFO) {
        return folly::LogLevel::INFO;
    }
    if (streamrLogLevel == StreamrLogLevel::WARN) {
        return folly::LogLevel::WARN;
    }
    if (streamrLogLevel == StreamrLogLevel::ERROR) {
        return folly::LogLevel::ERR;
    }
    return folly::LogLevel::CRITICAL;
}

// This function can be used at compile time
constexpr folly::LogLevel getFollyLogLevel(
    std::string_view streamrLogLevelName) {
    if (streamrLogLevelName == logLevelTraceName) {
        return folly::LogLevel::DBG;
    }
    if (streamrLogLevelName == logLevelDebugName) {
        return folly::LogLevel::DBG0;
    }
    if (streamrLogLevelName == logLevelInfoName) {
        return folly::LogLevel::INFO;
    }
    if (streamrLogLevelName == logLevelWarnName) {
        return folly::LogLevel::WARN;
    }
    if (streamrLogLevelName == logLevelErrorName) {
        return folly::LogLevel::ERR;
    }
    return folly::LogLevel::CRITICAL;
}
}; // namespace detail

class Logger {
private:
    StreamrWriterFactory mWriterFactory;
    folly::LoggerDB& mLoggerDB;
    std::unique_ptr<folly::LogHandlerFactory> mLogHandlerFactory;
    nlohmann::json mContextBindings;
    folly::LogLevel mDefaultLogLevel;

    folly::LogLevel getFollyLogRootLevel() {
        char* val = getenv(detail::envLogLevelName.data());
        if (val) {
            return detail::getFollyLogLevel(val);
        }
        return mDefaultLogLevel;
    }

    void initialize(std::shared_ptr<folly::LogWriter> logWriter) { // NOLINT
        if (logWriter) {
            mWriterFactory = StreamrWriterFactory(logWriter);
        } else {
            mWriterFactory = StreamrWriterFactory();
        }
        mLogHandlerFactory =
            std::make_unique<StreamrHandlerFactory>(&mWriterFactory);
        this->initializeLoggerDB(folly::LogLevel::DBG);
    }

    void changeToObjectIfNotStructured(
        nlohmann::json& element, const std::string_view name) {
        if (!element.is_structured()) {
            // Change to object if not structured
            if (element.is_string() && element.dump() == "\"\"") {
                // If empty string (default value when object is not needed),
                // then add empty object
                element = nlohmann::json::object({});
            } else {
                auto temp = element.dump();
                element = nlohmann::json::object({{name, element}});
            }
        }
    }

    template <typename T = std::string>
    void log(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const T& metadata,
        const std::source_location& location) {
        auto extraArgument = streamr::json::toJson(metadata);
        changeToObjectIfNotStructured(extraArgument, "metadata");
        extraArgument.merge_patch(mContextBindings);
        auto extraArgumentInString = getJsonObjectInString(extraArgument);
        logCommon(follyLogLevelLevel, msg, extraArgumentInString, location);
    }

    std::string getJsonObjectInString(const nlohmann::json& object) {
        if (object.empty()) {
            return ("");
        }
        // One space because it is needed after message
        return " " + streamr::json::toString(object);
    }

    void log(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const std::source_location& location) {
        auto extraArgumentInString{getJsonObjectInString(mContextBindings)};
        logCommon(follyLogLevelLevel, msg, extraArgumentInString, location);
    }

    template <typename T = std::string>
    void logCommon(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const std::string& metadata,
        const std::source_location& location) {
        auto follyRootLogLevel = getFollyLogRootLevel();
        if (follyRootLogLevel != mLoggerDB.getCategory("")->getLevel()) {
            this->initializeLoggerDB(follyRootLogLevel, true);
        }
        sendLogMessage(follyLogLevelLevel, msg, metadata, location);
    }

    void sendLogMessage(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const std::string& metadata,
        const std::source_location& location) {
        folly::LogStreamProcessor(
            [] {
                static ::folly::XlogCategoryInfo<XLOG_IS_IN_HEADER_FILE>
                    follyDetailXlogCategory;
                return follyDetailXlogCategory.getInfo(
                    &::folly::detail::custom::xlogFileScopeInfo);
            }(),
            (follyLogLevelLevel),
            [] {
                constexpr auto* follyDetailXlogFilename = XLOG_FILENAME;
                return ::folly::detail::custom::getXlogCategoryName(
                    follyDetailXlogFilename, 0);
            }(),
            ::folly::detail::custom::isXlogCategoryOverridden(0),
            location.file_name(),
            location.line(),
            __func__,
            ::folly::LogStreamProcessor::APPEND,
            msg,
            metadata)
            .stream();
    }

    void initializeLoggerDB(
        const folly::LogLevel& rootLogLevel,
        bool isSkipRegisterHandlerFactory = false) {
        if (!isSkipRegisterHandlerFactory) {
            mLoggerDB.registerHandlerFactory(
                std::move(mLogHandlerFactory), true);
        }
        auto rootLogLevelInString = folly::logLevelToString(rootLogLevel);
        auto defaultHandlerConfig = folly::LogHandlerConfig(
            "stream",
            {{"stream", "stderr"},
             {"async", "false"},
             {"level", rootLogLevelInString}});
        auto rootCategoryConfig =
            folly::LogCategoryConfig(rootLogLevel, false, {"default"});
        folly::LogConfig config(
            {{"default", defaultHandlerConfig}}, {{"", rootCategoryConfig}});
        mLoggerDB.updateConfig(config);
    }

public:
    template <typename T = std::string>
    explicit Logger(
        const T& contextBindings = std::string(""),
        detail::StreamrLogLevel defaultLogLevel = detail::StreamrLogLevel::INFO,
        std::shared_ptr<folly::LogWriter> logWriter = nullptr) // NOLINT
        : mDefaultLogLevel{detail::getFollyLogLevel(defaultLogLevel)},
          mLoggerDB{folly::LoggerDB::get()} {
        mContextBindings = streamr::json::toJson(contextBindings);
        changeToObjectIfNotStructured(mContextBindings, "contextBindings");
        initialize(std::move(logWriter));
    }

    template <typename T = std::string>
    void trace(
        const std::string& msg,
        const T& metadata,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::DBG, msg, metadata, location);
    }

    void trace(
        const std::string& msg,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::DBG, msg, location);
    }

    template <typename T = std::string>
    void debug(
        const std::string& msg,
        const T& metadata,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::DBG0, msg, metadata, location);
    }

    void debug(
        const std::string& msg,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::DBG0, msg, location);
    }

    template <typename T = std::string>
    void info(
        const std::string& msg,
        const T& metadata,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::INFO, msg, metadata, location);
    }

    void info(
        const std::string& msg,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::INFO, msg, location);
    }

    template <typename T = std::string>
    void warn(
        const std::string& msg,
        const T& metadata,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::WARN, msg, metadata, location);
    }

    void warn(
        const std::string& msg,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::WARN, msg, location);
    }

    template <typename T = std::string>
    void error(
        const std::string& msg,
        const T& metadata,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::ERR, msg, metadata, location);
    }

    void error(
        const std::string& msg,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::ERR, msg, location);
    }

    template <typename T = std::string>
    void fatal(
        const std::string& msg,
        const T& metadata,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::CRITICAL, msg, metadata, location);
    }

    void fatal(
        const std::string& msg,
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::CRITICAL, msg, location);
    }
};

}; // namespace streamr::logger

#endif
