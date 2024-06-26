#ifndef STREAMER_LOGGER_LOGGER_HPP
#define STREAMER_LOGGER_LOGGER_HPP

#include <source_location>
#include <string_view>
#include <nlohmann/json.hpp>
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
#include "streamr-logger/LogLevelMap.hpp"

namespace streamr::logger {

using streamr::json::toJson;
constexpr std::string_view envLogLevelName = "LOG_LEVEL";

class Logger {
private:
    StreamrWriterFactory mWriterFactory;
    folly::LoggerDB& mLoggerDB;
    std::unique_ptr<folly::LogHandlerFactory> mLogHandlerFactory;
    nlohmann::json mContextBindings;
    folly::LogLevel mDefaultLogLevel;

    static constexpr size_t logLevelMapSize = 6;
    static constexpr const LogLevelMap<logLevelMapSize>& getLogLevelMap() {
        // You can change the log level mapping
        // between Streamr and folly by editing
        // this magic static map
        static constexpr LogLevelMap<logLevelMapSize> logLevelMap(
            {{{{streamrloglevel::Trace{}, folly::LogLevel::DBG},
               {streamrloglevel::Debug{}, folly::LogLevel::DBG0},
               {streamrloglevel::Info{}, folly::LogLevel::INFO},
               {streamrloglevel::Warn{}, folly::LogLevel::WARN},
               {streamrloglevel::Error{}, folly::LogLevel::ERR},
               {streamrloglevel::Fatal{}, folly::LogLevel::CRITICAL}}}});
        return logLevelMap;
    }

public:
    // ContextBindingsType can be any type that is convertible to JSON by
    // streamr-json
    template <typename ContextBindingsType = std::string>
    explicit Logger(
        const ContextBindingsType& contextBindings = nlohmann::json{},
        StreamrLogLevel defaultLogLevel = streamrloglevel::Info{},
        std::shared_ptr<folly::LogWriter> logWriter =
            nullptr) // using std::optional will not work because
                     // folly::LogWriter is an abstract class
        : mLoggerDB{folly::LoggerDB::get()} {
        mDefaultLogLevel =
            getLogLevelMap().streamrLevelToFollyLevel(defaultLogLevel);
        mContextBindings =
            ensureJsonObject(toJson(contextBindings), "contextBindings");
        initialize(std::move(logWriter));
    }

    template <typename T = std::string>
    void trace(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::DBG, msg, metadata, location);
    }

    template <typename T = std::string>
    void debug(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::DBG0, msg, metadata, location);
    }

    template <typename T = std::string>
    void info(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::INFO, msg, metadata, location);
    }

    template <typename T = std::string>
    void warn(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::WARN, msg, metadata, location);
    }

    template <typename T = std::string>
    void error(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::ERR, msg, metadata, location);
    }

    template <typename T = std::string>
    void fatal(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(folly::LogLevel::CRITICAL, msg, metadata, location);
    }

private:
    folly::LogLevel getFollyLogRootLevel() {
        char* val = getenv(envLogLevelName.data());
        if (val) {
            return getLogLevelMap().streamrLevelNameToFollyLevel(val);
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

    // Ensure that the given JSON element is an object
    // If it is not, then create an object and place the given element
    // under the given key in the object

    nlohmann::json ensureJsonObject(
        const nlohmann::json& element, const std::string_view key) {
        if (element.is_object()) {
            return element;
        }
        if (element.is_null() ||
            (element.is_string() && element.get<std::string>() == "")) {
            return nlohmann::json::object({});
        }
        return nlohmann::json::object({{key, element}});
    }

    // MetadataType can be any type that is convertible to JSON by
    // streamr-json
    template <typename MetadataType = std::string>
    void log(
        const folly::LogLevel follyLogLevelLevel,
        const std::string& msg,
        const MetadataType& metadata,
        const std::source_location& location) {
        auto metadataJson = ensureJsonObject(toJson(metadata), "metadata");
        metadataJson.merge_patch(mContextBindings);
        // auto extraArgumentInString = getJsonObjectInString(extraArgument);
        auto metadataString =
            metadataJson.empty() ? "" : (" " + metadataJson.dump());
        auto follyRootLogLevel = getFollyLogRootLevel();
        if (follyRootLogLevel != mLoggerDB.getCategory("")->getLevel()) {
            this->initializeLoggerDB(follyRootLogLevel, true);
        }
        sendLogMessage(follyLogLevelLevel, msg, metadataString, location);
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
};

}; // namespace streamr::logger

#endif
