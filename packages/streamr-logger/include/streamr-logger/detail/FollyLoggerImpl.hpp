#ifndef STREAMR_LOGGER_DETAIL_FOLLY_LOGGER_IMPL_HPP
#define STREAMR_LOGGER_DETAIL_FOLLY_LOGGER_IMPL_HPP

#include <source_location>
#include <folly/logging/LogConfig.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include <folly/logging/LogWriter.h>
#include <folly/logging/LoggerDB.h>
#include <folly/logging/xlog.h>

#include "LogLevelMap.hpp"
#include "StreamrHandlerFactory.hpp"
#include "StreamrWriterFactory.hpp"
#include "streamr-logger/LoggerImpl.hpp"

namespace streamr::logger::detail {

using LoggerImpl = streamr::logger::LoggerImpl;

class FollyLoggerImpl : public LoggerImpl {
private:
    StreamrWriterFactory mWriterFactory;
    folly::LoggerDB& mLoggerDB;
    std::unique_ptr<folly::LogHandlerFactory> mLogHandlerFactory;

public:
    explicit FollyLoggerImpl(
        std::shared_ptr<folly::LogWriter> logWriter = nullptr)
        : mWriterFactory(std::move(logWriter)),
          mLoggerDB{folly::LoggerDB::get()} {
        mLogHandlerFactory =
            std::make_unique<StreamrHandlerFactory>(&mWriterFactory);
        this->initializeLoggerDB(folly::LogLevel::DBG);
    }

    void sendLogMessage(
        const streamr::logger::StreamrLogLevel logLevel,
        const std::string& msg,
        const std::string& metadata,
        const std::source_location& location) override {
        const auto follyLogLevel =
            LogLevelMap::instance().streamrLevelToFollyLevel(logLevel);
        folly::LogStreamProcessor(
            [] {
                static ::folly::XlogCategoryInfo<XLOG_IS_IN_HEADER_FILE>
                    follyDetailXlogCategory;
                return follyDetailXlogCategory.getInfo(
                    &::folly::detail::custom::xlogFileScopeInfo);
            }(),
            (follyLogLevel),
            [] {
                constexpr auto* follyDetailXlogFilename = XLOG_FILENAME;
                return ::folly::detail::custom::getXlogCategoryName(
                    follyDetailXlogFilename, 0);
            }(),
            ::folly::detail::custom::isXlogCategoryOverridden(0),
            location.file_name(),
            location.line(),
            location.function_name(),
            ::folly::LogStreamProcessor::APPEND,
            msg,
            metadata)
            .stream();
    }

private:
    void initializeLoggerDB(const folly::LogLevel& rootLogLevel) {
        mLoggerDB.registerHandlerFactory(std::move(mLogHandlerFactory), true);

        auto rootLogLevelInString = folly::logLevelToString(rootLogLevel);
        auto defaultHandlerConfig = folly::LogHandlerConfig(
            "stream",
            {{"stream", "stdout"},
             {"async", "false"},
             {"level", rootLogLevelInString}});
        auto rootCategoryConfig =
            folly::LogCategoryConfig(rootLogLevel, false, {"default"});
        folly::LogConfig config(
            {{"default", defaultHandlerConfig}}, {{"", rootCategoryConfig}});
        mLoggerDB.updateConfig(config);
    }
};

} // namespace streamr::logger::detail

#endif