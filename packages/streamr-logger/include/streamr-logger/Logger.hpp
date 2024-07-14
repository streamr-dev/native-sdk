#ifndef STREAMER_LOGGER_LOGGER_HPP
#define STREAMER_LOGGER_LOGGER_HPP

#include <initializer_list>
#include <source_location>
#include <string_view>
#include <nlohmann/json.hpp>
#include "streamr-json/toJson.hpp"
#include "streamr-logger/LoggerImpl.hpp"
#include "streamr-logger/StreamrLogLevel.hpp"
#include "streamr-logger/detail/FollyLoggerImpl.hpp"

namespace streamr::logger {

using streamr::json::AssignableToJsonBuilder;
using streamr::json::JsonBuilder;
using streamr::json::toJson;

constexpr std::string_view envLogLevelName = "LOG_LEVEL";
class Logger {
private:
    std::shared_ptr<LoggerImpl> mLoggerImpl;
    StreamrLogLevel mLoggerLogLevel;
    nlohmann::json mContextBindings;

public:
    /**
     * @brief Construct a new Logger object, use this constructor only when
     * creating context-dependant loggers, for example a logger for a specific
     * loop. For normal use, use the static instance() method instead.
     *
     * @param contextBindings   (any type except classes/structs with private
     * sections) Context bindings to be added to every log message
     * @param defaultLogLevel   Default log level to use if no log level is set
     * in env variables.
     * @param loggerImpl        Logger implementation to use, if not set, the
     * default (Folly) implementation is used
     */

    // ContextBindingsType can be any type that is convertible to JSON by
    // streamr-json (= any type without private sections)
    template <typename ContextBindingsType = std::initializer_list<JsonBuilder>>
    explicit Logger(
        ContextBindingsType contextBindings = {},
        StreamrLogLevel defaultLogLevel = systemDefaultLogLevel,
        std::shared_ptr<LoggerImpl> loggerImpl = nullptr) {
        // If LOG_LEVEL env variable is set and is valid,
        // use it as the default log level for this logger.
        // Otherwise, use the defaultLogLevel.

        char* val = getenv(envLogLevelName.data());
        if (val) {
            mLoggerLogLevel = getStreamrLogLevelByName(val, defaultLogLevel);
        } else {
            mLoggerLogLevel = defaultLogLevel;
        }

        mContextBindings =
            ensureJsonObject(toJson(contextBindings), "contextBindings");

        if (!loggerImpl) {
            mLoggerImpl = std::make_shared<detail::FollyLoggerImpl>();
        } else {
            mLoggerImpl = std::move(loggerImpl);
        }

        mLoggerImpl->init(mLoggerLogLevel);
    }

    /**
     * @brief Get the default logger instance with default settings.
     *
     * @return Logger&
     */

    static Logger& instance() {
        // magic static ensures that the logger is created only once
        static Logger instance;
        return instance;
    }

    /**
     * @brief Log a message at the trace level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = std::initializer_list<JsonBuilder>>
    void trace(
        const char* msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Trace{}, msg, metadata, location);
    }

    /**
     * @brief Log a message at the debug level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = std::initializer_list<JsonBuilder>>
    void debug(
        const char* msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Debug{}, msg, metadata, location);
    }

    /**
     * @brief Log a message at the info level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = std::initializer_list<JsonBuilder>>
    void info(
        const char* msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Info{}, msg, metadata, location);
    }

    /**
     * @brief Log a message at the warn level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = std::initializer_list<JsonBuilder>>
    void warn(
        const char* msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Warn{}, msg, metadata, location);
    }

    /**
     * @brief Log a message at the error level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = std::initializer_list<JsonBuilder>>
    void error(
        const char* msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Error{}, msg, metadata, location);
    }

    /**
     * @brief Log a message at the fatal level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = std::initializer_list<JsonBuilder>>
    void fatal(
        const char* msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        log(streamrloglevel::Fatal{}, msg, metadata, location);
    }

private:
    // Ensure that the given JSON element is an object.
    // If it is not, create an object and place the given element
    // under the given key in the object

    static nlohmann::json ensureJsonObject(
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
    template <typename MetadataType>
    void log(
        const StreamrLogLevel messageLogLevel,
        const std::string& msg,
        MetadataType metadata,
        const std::source_location& location) {
        // Merge the possible metadata with the context bindings

        auto metadataJson = ensureJsonObject(toJson(metadata), "metadata");
        metadataJson.merge_patch(mContextBindings);
        auto metadataString =
            metadataJson.empty() ? "" : (" " + metadataJson.dump());

        mLoggerImpl->sendLogMessage(
            messageLogLevel, msg, metadataString, location);
    }
};

}; // namespace streamr::logger

#endif
