#ifndef STREAMR_LOGGER_DETAIL_FOLLY_LOGGER_IMPL_HPP
#define STREAMR_LOGGER_DETAIL_FOLLY_LOGGER_IMPL_HPP

#include <unistd.h>
#include <memory>
#include <source_location>
#include <folly/logging/LogCategoryConfig.h>
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

extern char** environ; // NOLINT (readability-redundant-declaration)
namespace streamr::logger::detail {

using LoggerImpl = streamr::logger::LoggerImpl;

constexpr std::string_view envCategoryLogLevelName = "LOG_LEVEL_";
class FollyLoggerImpl : public LoggerImpl {
private:
    std::shared_ptr<StreamrWriterFactory> mWriterFactory;
    folly::LoggerDB& mLoggerDB;
    std::unique_ptr<folly::LogHandlerFactory> mLogHandlerFactory;

public:
    explicit FollyLoggerImpl(
        std::shared_ptr<folly::LogWriter> logWriter = nullptr)
        : mLoggerDB{folly::LoggerDB::get()} {
        mWriterFactory =
            std::make_shared<StreamrWriterFactory>(std::move(logWriter));
        mLogHandlerFactory =
            std::make_unique<StreamrHandlerFactory>(mWriterFactory.get());
    }

    void init(const streamr::logger::StreamrLogLevel logLevel) override {
        auto loggerFollyLogLevel =
            LogLevelMap::instance().streamrLevelToFollyLevel(logLevel);
        this->initializeLoggerDB(loggerFollyLogLevel);
    }

    void sendLogMessage(
        const streamr::logger::StreamrLogLevel logLevel,
        const std::string& msg,
        const std::string& metadata,
        const std::source_location& location) override {
        const auto messageFollyLogLevel =
            LogLevelMap::instance().streamrLevelToFollyLevel(logLevel);

        auto fileCategoryName =
            folly::getXlogCategoryNameForFile(location.file_name());

        this->setFileLogCategoriesFromEnv(std::string(fileCategoryName));

        auto* fileCategory = mLoggerDB.getCategory(fileCategoryName);

        folly::LogStreamProcessor(
            fileCategory,
            messageFollyLogLevel,
            location.file_name(),
            location.line(),
            location.function_name(),
            ::folly::LogStreamProcessor::APPEND,
            msg,
            metadata)
            .stream();
    }

private:
    void setFileLogCategoriesFromEnv(const std::string& fileCategory) {
        // go through all env variables and find all that
        // start with LOG_LEVEL_
        for (char** env = environ; *env != nullptr; ++env) {
            const std::string envVar = *env;

            if (envVar.find(envCategoryLogLevelName) == 0) {
                std::string envCategory = envVar.substr(
                    envCategoryLogLevelName.size(),
                    envVar.find('=') - envCategoryLogLevelName.size());
                std::string envValue = envVar.substr(envVar.find('=') + 1);
                auto categoryLogLevel =
                    LogLevelMap::instance().streamrLevelNameToFollyLevel(
                        envValue);
                // fileCategories are dot-separated full file paths
                // envCategories are simple words such as 'MyComponent'
                // check if envCategory partially matches fileCategory

                auto pos = fileCategory.rfind(envCategory);

                if (pos != std::string::npos) {
                    // create matching category name from the beginning
                    // of the fileCategory, for example
                    // fileCategory: "root.myproject.include.MyClass.cpp"
                    // envCategory: "myproject"
                    // matchingCategoryName: "root.myproject"

                    auto matchingCategoryName =
                        fileCategory.substr(0, pos + envCategory.size());

                    if (!mLoggerDB.getCategoryOrNull(matchingCategoryName)) {
                        // if category is not previously added, add it

                        auto categoryLogLevelAsString =
                            folly::logLevelToString(categoryLogLevel);
                        auto newHandlerConfig = folly::LogHandlerConfig(
                            "stream",
                            {{"stream", "stdout"},
                             {"async", "false"},
                             {"level", categoryLogLevelAsString}});

                        folly::LogConfig::CategoryConfigMap newCategoryConfigs;
                        newCategoryConfigs[matchingCategoryName] =
                            folly::LogCategoryConfig(
                                categoryLogLevel,
                                false,
                                {matchingCategoryName});
                        newCategoryConfigs[matchingCategoryName]
                            .propagateLevelMessagesToParent =
                            folly::LogLevel::MAX_LEVEL;

                        folly::LogConfig config(
                            {{matchingCategoryName, newHandlerConfig}},
                            newCategoryConfigs);

                        mLoggerDB.updateConfig(config);
                    }
                }
            }
        }
    }

    void initializeLoggerDB(const folly::LogLevel& rootLogLevel) {
        mLoggerDB.registerHandlerFactory(std::move(mLogHandlerFactory), true);

        auto rootLogLevelAsString = folly::logLevelToString(rootLogLevel);
        auto defaultHandlerConfig = folly::LogHandlerConfig(
            "stream",
            {{"stream", "stdout"},
             {"async", "false"},
             {"level", rootLogLevelAsString}});

        auto rootCategoryConfig =
            folly::LogCategoryConfig(rootLogLevel, false, {"default"});

        folly::LogConfig config(
            {{"default", defaultHandlerConfig}}, {{"", rootCategoryConfig}});

        mLoggerDB.updateConfig(config);
    }
};

} // namespace streamr::logger::detail

#endif