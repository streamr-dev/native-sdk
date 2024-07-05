#ifndef STREAMER_LOGGER_LOGGER_HPP
#define STREAMER_LOGGER_LOGGER_HPP

#include <source_location>
#include <string_view>
#include <nlohmann/json.hpp>
#include "streamr-json/toJson.hpp"
#include "streamr-logger/LoggerImpl.hpp"
#include "streamr-logger/StreamrLogLevel.hpp"
#include "streamr-logger/detail/FollyLoggerImpl.hpp"

namespace streamr::logger {

using streamr::json::toJson;
constexpr std::string_view envLogLevelName = "LOG_LEVEL";

class Logger {
private:
    StreamrLogLevel mDefaultLogLevel;
    std::shared_ptr<LoggerImpl> mLoggerImpl;
    nlohmann::json mContextBindings;

public:
    // ContextBindingsType can be any type that is convertible to JSON by
    // streamr-json, ToDo: use a Concept to enforce this
    template <typename ContextBindingsType = std::string>
    explicit Logger(
        const ContextBindingsType& contextBindings = nlohmann::json{},
        StreamrLogLevel defaultLogLevel = streamrloglevel::Info{},
        std::shared_ptr<LoggerImpl> loggerImpl = nullptr)
        : mDefaultLogLevel(defaultLogLevel) {
        mContextBindings =
            ensureJsonObject(toJson(contextBindings), "contextBindings");

        if (!loggerImpl) {
            loggerImpl = std::make_shared<detail::FollyLoggerImpl>();
        } else {
            mLoggerImpl = std::move(loggerImpl);
        }
    }

    template <typename T = std::string>
    void trace(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Trace{}, msg, metadata, location);
    }

    template <typename T = std::string>
    void debug(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Debug{}, msg, metadata, location);
    }

    template <typename T = std::string>
    void info(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Info{}, msg, metadata, location);
    }

    template <typename T = std::string>
    void warn(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Warn{}, msg, metadata, location);
    }

    template <typename T = std::string>
    void error(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Error{}, msg, metadata, location);
    }

    template <typename T = std::string>
    void fatal(
        const std::string& msg,
        const T& metadata = std::string(""),
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Fatal{}, msg, metadata, location);
    }

private:
    StreamrLogLevel getLoggerLogLevel(std::string_view fileName) {
        // If per-file log level setting is found in env
        // varibale of the type 'LOG_LEVEL_example.cpp=trace'
        // return the corresponding StreamrLogLevel.
        // If the value of the env variable exists but is not valid,
        // return the default log level which is Info.

        std::string perFileEnvName(envLogLevelName);
        perFileEnvName = +"_" + std::string(fileName);

        char* fileVal = getenv(perFileEnvName.c_str());
        if (fileVal) {
            return getStreamrLogLevelByName(fileVal);
        }

        // If no per-file log level setting is found,
        // return the log level set by 'LOG_LEVEL' env variable.
        // If 'LOG_LEVEL' is not valid, return the default log level
        // which is Info.

        char* val = getenv(envLogLevelName.data());
        if (val) {
            return getStreamrLogLevelByName(val);
        }

        // If neither env variable is set, return the default
        // log level of this Logger instance.

        return mDefaultLogLevel;
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
        const StreamrLogLevel messageLogLevel,
        const std::string& msg,
        const MetadataType& metadata,
        const std::source_location& location) {
        // Merge the possible metadata with the context bindings

        auto metadataJson = ensureJsonObject(toJson(metadata), "metadata");
        metadataJson.merge_patch(mContextBindings);
        auto metadataString =
            metadataJson.empty() ? "" : (" " + metadataJson.dump());

        // Get the log level of this logger, using the filename as a hint
        auto loggerLogLevel = getLoggerLogLevel(location.file_name());

        // only send the message if the log level of this logger is less or
        // equal to the log level of the message

        if (getStreamrLogLevelValue(loggerLogLevel) <=
            getStreamrLogLevelValue(messageLogLevel)) {
            mLoggerImpl->sendLogMessage(
                messageLogLevel, msg, metadataString, location);
        }
    }
};

}; // namespace streamr::logger

#endif
