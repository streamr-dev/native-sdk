#ifndef STREAMER_UTILS_LOGGER_LOGGER_H
#define STREAMER_UTILS_LOGGER_LOGGER_H

#include <folly/logging/Logger.h>
#include <folly/logging/xlog.h>

#include <iostream>
#include <memory>
#include <folly/Format.h>
#include <folly/detail/StaticSingletonManager.h>
#include <folly/logging/FileHandlerFactory.h>
#include <folly/logging/LogCategory.h>
#include <folly/logging/LogCategoryConfig.h>
#include <folly/logging/LogConfig.h>
#include <folly/logging/LogConfigParser.h>
#include <folly/logging/LogFormatter.h>
#include <folly/logging/LogHandler.h>
#include <folly/logging/LogHandlerConfig.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include <folly/logging/LogWriter.h>
#include <folly/logging/LoggerDB.h>
#include <folly/logging/StandardLogHandler.h>
#include <folly/logging/StandardLogHandlerFactory.h>
#include <folly/logging/StreamHandlerFactory.h>
#include <folly/logging/xlog.h>
#include <folly/portability/Time.h>

namespace streamr {
namespace logger {

namespace {

static const auto envLogLevelName = "LOG_LEVEL";
static const auto logLevelTraceText = "trace";
static const auto logLevelDebugText = "debug";
static const auto logLevelInfoText = "info";
static const auto logLevelWarnText = "warn";
static const auto logLevelErrorText = "error";
static const auto logLevelFatalText = "fatal";

const std::unordered_map<std::string, folly::LogLevel> toFollyLogLevelMap{
    {logLevelTraceText, folly::LogLevel::DBG},
    {logLevelDebugText, folly::LogLevel::DBG0},
    {logLevelInfoText, folly::LogLevel::INFO},
    {logLevelWarnText, folly::LogLevel::WARN},
    {logLevelErrorText, folly::LogLevel::ERR},
    {logLevelFatalText, folly::LogLevel::FATAL}};

struct StreamrLogMessage {
    std::chrono::system_clock::time_point timestamp;
    folly::StringPiece fileBasename;
    unsigned int lineNumber;
    folly::LogLevel logLevel;
    const std::string& logMessage;
};

}; // namespace
/*
Format based on this Streamr log. The Filename and line number is fixed size
with 36 characters. If it is longer then it is truncated
DEBUG [2024-06-06T13:52:49.816] (<Filename>: <Line Number>     ):<Message>

Examples of a truncated filename
ERROR [2024-06-06T13:52:49.816] (12345678901234567890123456789012: 10):<Message>
ERROR [2024-06-06T13:52:49.816] (1234567890123456789012345678901: 101):<Message>

This one is not truncated:
ERROR [2024-06-06T13:52:49.816] (1234567890123456.cpp: 101           ):<Message>

DEBUG [2024-06-05T08:50:39.787] (File name): Message
ERROR [2024-06-05T08:50:39.787] (File name): Message
FATAL [2024-06-05T08:50:39.787] (File name): Message
INFO [2024-06-05T08:50:39.787] (File name): Message
TRACE [2024-06-05T08:50:39.787] (File name): Message
WARN [2024-06-05T08:50:39.787] (File name): Message
*/

class StreamrLogFormatter : public folly::LogFormatter {
   public:
    StreamrLogFormatter() = default;
    ~StreamrLogFormatter() override = default;

    struct StreamrLogMessage {
        std::chrono::system_clock::time_point timestamp;
        folly::StringPiece fileBasename;
        unsigned int lineNumber;
        folly::LogLevel logLevel;
        const std::string& logMessage;
    };

    std::string formatMessageInStreamrStyle(const StreamrLogMessage& message) {
        struct tm ltime;
        const auto timeSinceEpoch = message.timestamp.time_since_epoch();
        const auto epochSeconds =
            std::chrono::duration_cast<std::chrono::seconds>(timeSinceEpoch);
        const std::chrono::milliseconds millisecs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                timeSinceEpoch) -
            epochSeconds;
        time_t unixTimestamp = epochSeconds.count();
        if (!localtime_r(&unixTimestamp, &ltime)) {
            memset(&ltime, 0, sizeof(ltime));
        }
        auto basename = message.fileBasename;
        const auto fileNameLength = std::ssize(basename);
        const auto lineNumberInString = std::to_string(message.lineNumber);
        const auto lineNumberLength = std::ssize(lineNumberInString);
        const std::string logMessageColor = "\033[36m";
        const std::string logMessageColorReset = "\033[0m";
        const auto fileNameAndLineNumberLength =
            (fileNameLength + lineNumberLength + separatorLength);
        // Is filename is truncated if filename, separator and lineNumber does
        // not fit to the fixed size
        if (fileNameAndLineNumberLength <= maxFileNameAndLineNumberLength) {
            basename = basename.toString()
                           .append(fileNameAndLineNumberSeparator)
                           .append(lineNumberInString);
            auto logLine = folly::sformat(
                "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({:<36}): {}{}{}\n",
                getColorSequence(message.logLevel),
                getLogLevelName(message.logLevel),
                logMessageColorReset,
                ltime.tm_year + 1900,
                ltime.tm_mon + 1,
                ltime.tm_mday,
                ltime.tm_hour,
                ltime.tm_min,
                ltime.tm_sec,
                millisecs.count(),
                basename,
                logMessageColor,
                message.logMessage,
                logMessageColorReset);
            return logLine;
        } else {
            // Truncate needed
            auto lengthForTruncatedFileName = maxFileNameAndLineNumberLength -
                (lineNumberLength + separatorLength);
            basename = basename.substr(0, lengthForTruncatedFileName);
            auto logLine = folly::sformat(
                "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({: <*}{}{}): {}{}{}\n",
                getColorSequence(message.logLevel),
                getLogLevelName(message.logLevel),
                logMessageColorReset,
                ltime.tm_year + 1900,
                ltime.tm_mon + 1,
                ltime.tm_mday,
                ltime.tm_hour,
                ltime.tm_min,
                ltime.tm_sec,
                millisecs.count(),
                lengthForTruncatedFileName,
                basename,
                fileNameAndLineNumberSeparator,
                lineNumberInString,
                logMessageColor,
                message.logMessage,
                logMessageColorReset);
            return logLine;
        }
    }

    std::string formatMessage(
        const folly::LogMessage& message,
        const folly::LogCategory* handlerCategory) override {
        return this->formatMessageInStreamrStyle(
            {message.getTimestamp(),
             message.getFileBaseName(),
             message.getLineNumber(),
             message.getLevel(),
             message.getMessage()});
    }

   private:
    static constexpr int maxFileNameAndLineNumberLength{36};
    static constexpr folly::StringPiece fileNameAndLineNumberSeparator{": "};
    static constexpr auto separatorLength{
        std::ssize(fileNameAndLineNumberSeparator)};

    constexpr folly::StringPiece getLogLevelName(folly::LogLevel level) {
        if (level == folly::LogLevel::DBG) {
            return "TRACE";
        } else if (level == folly::LogLevel::DBG0) {
            return "DEBUG";
        } else if (level == folly::LogLevel::INFO) {
            return "INFO";
        } else if (level == folly::LogLevel::WARN) {
            return "WARN";
        } else if (level == folly::LogLevel::ERR) {
            return "ERROR";
        }
        return "FATAL";
    }

    folly::StringPiece getColorSequence(folly::LogLevel level) {
        if (level == folly::LogLevel::DBG) {
            return "\033[90m"; // Gray (TRACE)
        } else if (level == folly::LogLevel::DBG0) {
            return "\033[34m"; // Blue (DEBUG)
        } else if (level == folly::LogLevel::INFO) {
            return "\033[32m"; // Green (INFO)
        } else if (level == folly::LogLevel::WARN) {
            return "\033[33m"; // Yellow (WARN)
        } else if (level == folly::LogLevel::ERR) {
            return "\033[31m"; // Red (ERROR)
        }
        return "\033[1;41m"; // Red Background (FATAL)
    }
};

class StreamrLogFormatterFactory
    : public folly::StandardLogHandlerFactory::FormatterFactory {
   public:
    bool processOption(
        folly::StringPiece name, folly::StringPiece value) override {
        return false;
    }

    std::shared_ptr<folly::LogFormatter> createFormatter(
        const std::shared_ptr<folly::LogWriter>&) override {
        return std::make_shared<StreamrLogFormatter>();
    }
};

class StreamrHandlerFactory : public folly::StreamHandlerFactory {
   public:
    StreamrHandlerFactory() = default;
    ~StreamrHandlerFactory() override = default;

    std::shared_ptr<folly::LogHandler> createHandler(
        const Options& options) override {
        StreamHandlerFactory::WriterFactory writerFactory;
        StreamrLogFormatterFactory formatterFactory;

        return folly::StandardLogHandlerFactory::createHandler(
            getType(), &writerFactory, &formatterFactory, options);
    }
};

class Logger {
   public:
    Logger() : loggerDB{folly::LoggerDB::get()} {
        this->initializeLoggerDB(loggerDB);
    }

    static Logger& get() {
        static Logger instance;
        return instance;
    }

    // Used only for unit testing
    Logger(folly::LoggerDB &loggerDB, bool isInitialized = true) : loggerDB{loggerDB} {
      if (isInitialized) {
         this->initializeLoggerDB(loggerDB);
      }
    }
    // Returns if log sent
    bool log(folly::LogLevel level, const std::string& message) {
        bool logSent = false;
        auto follyLogLevel = getFollyLogLevelFromEnv();
        if (follyLogLevel) {
            if (follyLogLevel != loggerDB.getCategory("")->getLevel()) {
                this->initializeLoggerDB(loggerDB, *follyLogLevel, true);
            }
            folly::LogStreamProcessor(
                [] {
                    static ::folly::XlogCategoryInfo<XLOG_IS_IN_HEADER_FILE>
                        folly_detail_xlog_category;
                    return folly_detail_xlog_category.getInfo(
                        &::folly::detail::custom::xlogFileScopeInfo);
                }(),
                (level),
                [] {
                    constexpr auto* folly_detail_xlog_filename = XLOG_FILENAME;
                    return ::folly::detail::custom::getXlogCategoryName(
                        folly_detail_xlog_filename, 0);
                }(),
                ::folly::detail::custom::isXlogCategoryOverridden(0),
                XLOG_FILENAME,
                __LINE__,
                __func__,
                ::folly::LogStreamProcessor::APPEND,
                message).stream();
                logSent = true;
        }
        return logSent;
    }

   private:
    folly::LoggerDB& loggerDB;

    std::optional<folly::LogLevel> getFollyLogLevelFromEnv() {
        char* val = getenv(envLogLevelName);
        auto isLogged = false;
        if (val) {
            auto pair = toFollyLogLevelMap.find(val);
            if (pair != toFollyLogLevelMap.end()) {
                return std::optional<folly::LogLevel>{std::move(pair->second)};
            }
        }
        return std::nullopt;
    }

    void initializeLoggerDB(
        folly::LoggerDB& db,
        const folly::LogLevel& rootLogLevel = folly::LogLevel::MIN_LEVEL,
        bool isSkipRegisterHandlerFactory = false) {
        if (!isSkipRegisterHandlerFactory) {
            db.registerHandlerFactory(
                std::make_unique<StreamrHandlerFactory>(), true);
        }
        auto defaultHandlerConfig = folly::LogHandlerConfig(
            "stream", {{"stream", "stderr"}, {"async", "false"}});
        auto rootCategoryConfig = folly::LogCategoryConfig(
            folly::LogLevel::MIN_LEVEL, false, {"default"});
        folly::LogConfig config(
            {{"default", defaultHandlerConfig}}, {{"", rootCategoryConfig}});
        db.updateConfig(config);
    }
};

#define SLOG_TRACE(msg) Logger::get().log(folly::LogLevel::DBG, msg)
#define SLOG_DEBUG(msg) Logger::get().log(folly::LogLevel::DBG0, msg)
#define SLOG_INFO(msg) Logger::get().log(folly::LogLevel::INFO, msg)
#define SLOG_WARN(msg) Logger::get().log(folly::LogLevel::WARN, msg)
#define SLOG_ERROR(msg) Logger::get().log(folly::LogLevel::ERR, msg)
#define SLOG_FATAL(msg) Logger::get().log(folly::LogLevel::FATAL, msg)

}; // namespace logger
}; // namespace streamr

#endif
