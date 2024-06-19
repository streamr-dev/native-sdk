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
    std::string filename_;

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
            if (element.is_string() && element.dump() == "\"\"") {
                // If empty string (default value when object is not needed), then
                // add empty object
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
        const int lineNumber) {
        auto extraArgument = streamr::json::toJson(metadata);
        changeToObjectIfNotStructured(extraArgument, "metadata");
        extraArgument.merge_patch(contextBindings_);
        auto extraArgumentInString = getJsonObjectInString(extraArgument);
        logCommon(follyLogLevelLevel, msg, extraArgumentInString, lineNumber);
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
        const int lineNumber) {
        auto extraArgumentInString{getJsonObjectInString(contextBindings_)};
        logCommon(follyLogLevelLevel, msg, extraArgumentInString, lineNumber);
    }

    template <typename T = std::string>
    void logCommon(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const std::string& metadata,
        const int lineNumber) {
        auto follyRootLogLevel = getFollyLogRootLevel();
        if (follyRootLogLevel != loggerDB_.getCategory("")->getLevel()) {
            this->initializeLoggerDB(follyRootLogLevel, true);
        }
        sendLogMessage(follyLogLevelLevel, msg, metadata, lineNumber);
    }

    void sendLogMessage(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const std::string& metadata,
        const int lineNumber) {
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
            filename_,
            lineNumber,
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
        const std::string_view filename,
        const T& contextBindings = std::string(""),
        detail::StreamrLogLevel defaultLogLevel = detail::StreamrLogLevel::INFO,
        std::shared_ptr<folly::LogWriter> logWriter = nullptr) // NOLINT
        : filename_{filename},
          defaultLogLevel_{detail::getFollyLogLevel(defaultLogLevel)},
          loggerDB_{folly::LoggerDB::get()} {
        contextBindings_ = streamr::json::toJson(contextBindings);
        changeToObjectIfNotStructured(contextBindings_, "contextBindings");
        initialize(std::move(logWriter));
    }

    template <typename T = std::string>
    static Logger& instance(
        std::string_view filename,
        const T& contextBindings = std::string(""),
        detail::StreamrLogLevel defaultLogLevel = detail::StreamrLogLevel::INFO,
        std::shared_ptr<folly::LogWriter> logWriter = nullptr) {
        static Logger loggerInstance{
            filename, contextBindings, defaultLogLevel, logWriter};
        return loggerInstance;
    }

    template <typename T = std::string>
    void logTrace(
        const std::string& msg, const int lineNumber, const T& metadata) {
        log(folly::LogLevel::DBG, msg, metadata, lineNumber);
    }

    void logTrace(const std::string& msg, const int lineNumber) {
        log(folly::LogLevel::DBG, msg, lineNumber);
    }

    template <typename T = std::string>
    void logDebug(
        const std::string& msg, const int lineNumber, const T& metadata) {
        log(folly::LogLevel::DBG0, msg, metadata, lineNumber);
    }

    void logDebug(const std::string& msg, const int lineNumber) {
        log(folly::LogLevel::DBG0, msg, lineNumber);
    }

    template <typename T = std::string>
    void logInfo(
        const std::string& msg, const int lineNumber, const T& metadata) {
        log(folly::LogLevel::INFO, msg, metadata, lineNumber);
    }

    void logInfo(const std::string& msg, const int lineNumber) {
        log(folly::LogLevel::INFO, msg, lineNumber);
    }

    template <typename T = std::string>
    void logWarn(
        const std::string& msg, const int lineNumber, const T& metadata) {
        log(folly::LogLevel::WARN, msg, metadata, lineNumber);
    }

    void logWarn(const std::string& msg, const int lineNumber) {
        log(folly::LogLevel::WARN, msg, lineNumber);
    }

    template <typename T = std::string>
    void logError(
        const std::string& msg, const int lineNumber, const T& metadata) {
        log(folly::LogLevel::ERR, msg, metadata, lineNumber);
    }

    void logError(const std::string& msg, const int lineNumber) {
        log(folly::LogLevel::ERR, msg, lineNumber);
    }

    template <typename T = std::string>
    void logFatal(
        const std::string& msg, const int lineNumber, const T& metadata) {
        log(folly::LogLevel::CRITICAL, msg, metadata, lineNumber);
    }

    void logFatal(const std::string& msg, const int lineNumber) {
        log(folly::LogLevel::CRITICAL, msg, lineNumber);
    }
};

}; // namespace streamr::logger

#define trace(msg, ...) \
    Logger::instance(__FILE__).logTrace(msg, __LINE__, ##__VA_ARGS__)
#define debug(msg, ...) \
    Logger::instance(__FILE__).logDebug(msg, __LINE__, ##__VA_ARGS__)
#define info(msg, ...) \
    Logger::instance(__FILE__).logInfo(msg, __LINE__, ##__VA_ARGS__)
#define warn(msg, ...) \
    Logger::instance(__FILE__).logWarn(msg, __LINE__, ##__VA_ARGS__)
#define error(msg, ...) \
    Logger::instance(__FILE__).logError(msg, __LINE__, ##__VA_ARGS__)
#define fatal(msg, ...) \
    Logger::instance(__FILE__).logFatal(msg, __LINE__, ##__VA_ARGS__)

#endif
