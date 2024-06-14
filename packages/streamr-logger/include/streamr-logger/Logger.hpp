#ifndef STREAMER_UTILS_LOGGER_LOGGER_H
#define STREAMER_UTILS_LOGGER_LOGGER_H

#include <folly/logging/LogConfig.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include <folly/logging/LogWriter.h>
#include <folly/logging/LoggerDB.h>
#include <folly/logging/xlog.h>
#include "StreamrHandlerFactory.hpp" 
#include "StreamrWriterFactory.hpp"

namespace streamr {
namespace logger {

namespace detail {

static const auto ENV_LOG_LEVEL_NAME = "LOG_LEVEL"; // NOLINT
static const auto LOG_LEVEL_TRACE_NAME = "trace";// NOLINT
static const auto LOG_LEVEL_DEBUG_NAME = "debug";// NOLINT
static const auto LOG_LEVEL_INFO_NAME = "info";// NOLINT
static const auto LOG_LEVEL_WARN_NAME = "warn";// NOLINT
static const auto LOG_LEVEL_ERROR_NAME = "error";// NOLINT
static const auto LOG_LEVEL_FATAL_NAME = "fatal";// NOLINT

enum class StreamrLogLevel { TRACE, DEBUG, INFO, WARN, ERROR, FATAL };  // NOLINT

const std::unordered_map<StreamrLogLevel, folly::LogLevel>
    FROM_STEAMR_TO_FOLLY_LOG_LEVEL_MAP{// NOLINT
        {StreamrLogLevel::TRACE, folly::LogLevel::DBG},
        {StreamrLogLevel::DEBUG, folly::LogLevel::DBG0},
        {StreamrLogLevel::INFO, folly::LogLevel::INFO}, 
        {StreamrLogLevel::WARN, folly::LogLevel::WARN},
        {StreamrLogLevel::ERROR, folly::LogLevel::ERR},
        {StreamrLogLevel::FATAL, folly::LogLevel::CRITICAL}};

const std::unordered_map<std::string, folly::LogLevel> TO_FOLLY_LEVEL_MAP{// NOLINT
    {LOG_LEVEL_TRACE_NAME, folly::LogLevel::DBG},
    {LOG_LEVEL_DEBUG_NAME, folly::LogLevel::DBG0},
    {LOG_LEVEL_INFO_NAME, folly::LogLevel::INFO},
    {LOG_LEVEL_WARN_NAME, folly::LogLevel::WARN},
    {LOG_LEVEL_ERROR_NAME, folly::LogLevel::ERR},
    {LOG_LEVEL_FATAL_NAME, folly::LogLevel::CRITICAL}};

}; // namespace

template <typename A>
class Logger {
public:
    explicit Logger(
        detail::StreamrLogLevel defaultLogLevel = detail::StreamrLogLevel::INFO,
        std::optional<A> contextBindings = std::nullopt,
        std::shared_ptr<folly::LogWriter> logWriter = nullptr)  // NOLINT
        : defaultLogLevel_{detail::FROM_STEAMR_TO_FOLLY_LOG_LEVEL_MAP.at(defaultLogLevel)},
          contextBindings_{contextBindings},
          loggerDB_{folly::LoggerDB::get()} {
        if (logWriter) {
            writerFactory_ = StreamrWriterFactory(logWriter); // NOLINT
        } else {
            writerFactory_ = StreamrWriterFactory();
        }
        logHandlerFactory_ = {
            std::make_unique<StreamrHandlerFactory>(&writerFactory_)};
        this->initializeLoggerDB(folly::LogLevel::DBG);
    }

    static Logger& get(std::shared_ptr<folly::LogWriter> logWriter = nullptr) {
        static Logger instance(
            streamr::logger::detail::StreamrLogLevel::INFO, std::nullopt, logWriter);
        return instance;
    }

    template <typename T>
    std::string toString(T metadata) {
        return metadata;
    }

    template <typename T>
    void info(const std::string& msg, std::optional<T> metadata) {
        log(folly::LogLevel::INFO, msg, metadata);
    }

private:
    std::shared_ptr<folly::LogWriter> logWriter_;
    StreamrWriterFactory writerFactory_;
    folly::LoggerDB& loggerDB_;
    std::unique_ptr<folly::LogHandlerFactory> logHandlerFactory_;
    std::optional<A> contextBindings_;
    folly::LogLevel defaultLogLevel_;

    folly::LogLevel getFollyLogRootLevel() {
        char* val = getenv(detail::ENV_LOG_LEVEL_NAME);
        if (val) {
            return detail::TO_FOLLY_LEVEL_MAP.at(val);
        } 
        return defaultLogLevel_;
    }

    template <typename T>
    void log(
        folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        std::optional<T> metadata) {
        auto follyRootLogLevel = getFollyLogRootLevel();
        if (follyRootLogLevel != loggerDB_.getCategory("")->getLevel()) {
            // loggerDB.setLevel("", *follyLogLevel);
            this->initializeLoggerDB(follyRootLogLevel, true);
        }
        std::string extraArgument;
        if (metadata) {
            extraArgument = extraArgument + *metadata;
            if (contextBindings_) {
                extraArgument = extraArgument + *contextBindings_;
            }
        }
        // This complicated code is mostly copy/pasted from folly's XLOG
        // macro. It is not very flexible to call macro from here so. This
        // code below prints a log message without any return code.
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
            extraArgument)
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
};

#define SLOG_TRACE(msg) Logger::get().log(StreamrLogLevel::TRACE, msg)
#define SLOG_DEBUG(msg) Logger::get().log(StreamrLogLevel::DEBUG, msg)
#define SLOG_INFO(msg) Logger::get().log(StreamrLogLevel::INFO, msg)
#define SLOG_WARN(msg) Logger::get().log(StreamrLogLevel::WARN, msg)
#define SLOG_ERROR(msg) Logger::get().log(StreamrLogLevel::ERROR, msg)
#define SLOG_FATAL(msg) Logger::get().log(StreamrLogLevel::FATAL, msg)

}; // namespace logger
}; // namespace streamr

#endif
