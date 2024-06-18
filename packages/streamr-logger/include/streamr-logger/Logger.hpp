#ifndef STREAMER_LOGGER_LOGGER_HPP
#define STREAMER_LOGGER_LOGGER_HPP

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
    StreamrWriterFactory writerFactory_;
    folly::LoggerDB& loggerDB_;
    std::unique_ptr<folly::LogHandlerFactory> logHandlerFactory_;
    nlohmann::json contextBindings_;
    folly::LogLevel defaultLogLevel_;

    folly::LogLevel getFollyLogRootLevel() {
        char* val = getenv(detail::envLogLevelName.data());
        if (val) {
            return detail::getFollyLogLevel(val);
        }
        return defaultLogLevel_;
    }

    void initialize(std::shared_ptr<folly::LogWriter> logWriter) { // NOLINT
        if (logWriter) {
            writerFactory_ = StreamrWriterFactory(logWriter);
        } else {
            writerFactory_ = StreamrWriterFactory();
        }
        logHandlerFactory_ =
            std::make_unique<StreamrHandlerFactory>(&writerFactory_);
        this->initializeLoggerDB(folly::LogLevel::DBG);
    }

    void changeToObjectIfNotStructured(
        nlohmann::json& element, const std::string_view name) {
        if (!element.is_structured()) {
            // Change to object if not structured
            element = nlohmann::json::object({{name, element}});
        }
    }

    template <typename T = std::string>
    void log(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const T& metadata) {
        auto extraArgument = streamr::json::toJson(metadata);
        changeToObjectIfNotStructured(extraArgument, "metadata");
        extraArgument.merge_patch(contextBindings_);
        auto extraArgumentInString = getJsonObjectInString(extraArgument);
        logCommon(follyLogLevelLevel, msg, extraArgumentInString);
    }

    std::string getJsonObjectInString(const nlohmann::json& object) {
        if (object.empty()) {
            return ("");
        }
        // One space because it is needed after message
        return " " + streamr::json::toString(object);
    }

    void log(const folly::LogLevel follyLogLevelLevel, const std::string& msg) {
        auto extraArgumentInString{getJsonObjectInString(contextBindings_)};
        logCommon(follyLogLevelLevel, msg, extraArgumentInString);
    }

    template <typename T = std::string>
    void logCommon(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const std::string& metadata) {
        auto follyRootLogLevel = getFollyLogRootLevel();
        if (follyRootLogLevel != loggerDB_.getCategory("")->getLevel()) {
            // loggerDB.setLevel("", *follyLogLevel);
            this->initializeLoggerDB(follyRootLogLevel, true);
        }
        sendLogMessage(follyLogLevelLevel, msg, metadata);
    }

    void sendLogMessage(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const std::string& metadata) {
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
            XLOG_FILENAME,
            __LINE__,
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
            loggerDB_.registerHandlerFactory(
                std::move(logHandlerFactory_), true);
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
        loggerDB_.updateConfig(config);
    }

public:
    template <typename T = std::string>
    explicit Logger(
        const T& contextBindings,
        detail::StreamrLogLevel defaultLogLevel = detail::StreamrLogLevel::INFO,
        std::shared_ptr<folly::LogWriter> logWriter = nullptr) // NOLINT
        : defaultLogLevel_{detail::getFollyLogLevel(defaultLogLevel)},
          loggerDB_{folly::LoggerDB::get()} {
        contextBindings_ = streamr::json::toJson(contextBindings);
        changeToObjectIfNotStructured(contextBindings_, "contextBindings");
        initialize(std::move(logWriter));
    }

    template <typename T = std::string>
    explicit Logger(
        detail::StreamrLogLevel defaultLogLevel = detail::StreamrLogLevel::INFO,
        std::shared_ptr<folly::LogWriter> logWriter = nullptr) // NOLINT
        : defaultLogLevel_{detail::getFollyLogLevel(defaultLogLevel)},
          loggerDB_{folly::LoggerDB::get()} {
        contextBindings_ = nlohmann::json::object({});
        initialize(std::move(logWriter));
    }

    template <typename T = std::string>
    void trace(const std::string& msg, const T& metadata) {
        log(folly::LogLevel::DBG, msg, metadata);
    }

    void trace(const std::string& msg) { log(folly::LogLevel::DBG, msg); }

    template <typename T = std::string>
    void debug(const std::string& msg, const T& metadata) {
        log(folly::LogLevel::DBG0, msg, metadata);
    }

    void debug(const std::string& msg) { log(folly::LogLevel::DBG0, msg); }

    template <typename T = std::string>
    void info(const std::string& msg, const T& metadata) {
        log(folly::LogLevel::INFO, msg, metadata);
    }

    void info(const std::string& msg) { log(folly::LogLevel::INFO, msg); }

    template <typename T = std::string>
    void warn(const std::string& msg, const T& metadata) {
        log(folly::LogLevel::WARN, msg, metadata);
    }

    void warn(const std::string& msg) { log(folly::LogLevel::WARN, msg); }

    template <typename T = std::string>
    void error(const std::string& msg, const T& metadata) {
        log(folly::LogLevel::ERR, msg, metadata);
    }

    void error(const std::string& msg) { log(folly::LogLevel::ERR, msg); }

    template <typename T = std::string>
    void fatal(const std::string& msg, const T& metadata) {
        log(folly::LogLevel::CRITICAL, msg, metadata);
    }

    void fatal(const std::string& msg) { log(folly::LogLevel::CRITICAL, msg); }
};

}; // namespace streamr::logger

#endif
