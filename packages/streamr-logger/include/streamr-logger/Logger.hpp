#ifndef STREAMER_UTILS_LOGGER_LOGGER_H
#define STREAMER_UTILS_LOGGER_LOGGER_H

#include <folly/logging/LogConfig.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include <folly/logging/LogWriter.h>
#include <folly/logging/LoggerDB.h>
#include <folly/logging/xlog.h>
#include "StreamrWriterFactory.hpp"
#include "StreamrHandlerFactory.hpp"

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

enum class StreamrLogLevel { trace, debug, info, warn, error, fatal };

const std::unordered_map<StreamrLogLevel, folly::LogLevel> fromStreamrToFollyLogLevelMap{
    {StreamrLogLevel::trace, folly::LogLevel::DBG},
    {StreamrLogLevel::debug, folly::LogLevel::DBG0},
    {StreamrLogLevel::info, folly::LogLevel::INFO},
    {StreamrLogLevel::warn, folly::LogLevel::WARN},
    {StreamrLogLevel::error, folly::LogLevel::ERR},
    {StreamrLogLevel::fatal, folly::LogLevel::CRITICAL}};

const std::unordered_map<std::string, folly::LogLevel> toFollyLogLevelMap{
    {logLevelTraceText, folly::LogLevel::DBG},
    {logLevelDebugText, folly::LogLevel::DBG0},
    {logLevelInfoText, folly::LogLevel::INFO},
    {logLevelWarnText, folly::LogLevel::WARN},
    {logLevelErrorText, folly::LogLevel::ERR},
    {logLevelFatalText, folly::LogLevel::CRITICAL}};

std::optional<folly::LogLevel> getFollyLogLevelFromEnv() {
    char* val = getenv(envLogLevelName);
    if (val) {
        auto pair = toFollyLogLevelMap.find(val);
        if (pair != toFollyLogLevelMap.end()) {
            return std::optional<folly::LogLevel>{std::move(pair->second)};
        }
    }
    return std::nullopt;
}

}; // namespace

class Logger {
public:

    Logger(std::shared_ptr<folly::LogWriter> logWriter = nullptr)
        : loggerDB_{folly::LoggerDB::get()} {
        if (logWriter) {
            writerFactory_ = StreamrWriterFactory(logWriter);
        } else {
            writerFactory_ = StreamrWriterFactory();
        }
        logHandlerFactory_ = {
            std::make_unique<StreamrHandlerFactory>(&writerFactory_)};
        this->initializeLoggerDB(folly::LogLevel::DBG);
    }

    static Logger& get(std::shared_ptr<folly::LogWriter> logWriter = nullptr) {
        static Logger instance(logWriter);
        return instance;
    }

    void log(StreamrLogLevel streamrLevel, const std::string& message) {
        auto follyRootLogLevel = getFollyLogLevelFromEnv();
        auto pair = fromStreamrToFollyLogLevelMap.find(streamrLevel);
        if (pair == fromStreamrToFollyLogLevelMap.end()) {
            // Not found from map, return
        }
        if (follyRootLogLevel) {
            if (follyRootLogLevel != loggerDB_.getCategory("")->getLevel()) {
                // loggerDB.setLevel("", *follyLogLevel);
                this->initializeLoggerDB(*follyRootLogLevel, true);
            }
            // This complicated code is mostly copy/pasted from folly's XLOG
            // macro. It is not very flexible to call macro from here so. This
            // code below prints a log message without any return code.
            folly::LogStreamProcessor(
                [] {
                    static ::folly::XlogCategoryInfo<XLOG_IS_IN_HEADER_FILE>
                        folly_detail_xlog_category;
                    return folly_detail_xlog_category.getInfo(
                        &::folly::detail::custom::xlogFileScopeInfo);
                }(),
                (pair->second),
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
                message)
                .stream();
        }
    }

private:
    std::shared_ptr<folly::LogWriter> logWriter_;
    StreamrWriterFactory writerFactory_;
    folly::LoggerDB& loggerDB_;
    std::unique_ptr<folly::LogHandlerFactory> logHandlerFactory_;
   
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

#define SLOG_TRACE(msg) Logger::get().log(StreamrLogLevel::trace, msg)
#define SLOG_DEBUG(msg) Logger::get().log(StreamrLogLevel::debug, msg)
#define SLOG_INFO(msg) Logger::get().log(StreamrLogLevel::info, msg)
#define SLOG_WARN(msg) Logger::get().log(StreamrLogLevel::warn, msg)
#define SLOG_ERROR(msg) Logger::get().log(StreamrLogLevel::error, msg)
#define SLOG_FATAL(msg) Logger::get().log(StreamrLogLevel::fatal, msg)

}; // namespace logger
}; // namespace streamr

#endif
