// Module streamr.logger.Logger
// CONSOLIDATED from the former header
// streamr-logger/Logger.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <algorithm>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <nlohmann/json.hpp>

// The C symbol for the process environment: must be declared in the
// global module fragment (see FollyLoggerImpl).
extern "C" char** environ; // NOLINT

export module streamr.logger.Logger;

import streamr.json.toJson;
import streamr.logger.FollyLoggerImpl;
import streamr.logger.LoggerImpl;
import streamr.logger.StreamrLogLevel;

export namespace streamr::logger {

using streamr::json::StreamrJsonInitializerList;
using streamr::json::toJson;

// const char* (not string_view): passed to getenv(), which needs a
// null-terminated string.
inline constexpr const char* envLogLevelName = "LOG_LEVEL";
class Logger {
private:
    std::shared_ptr<LoggerImpl> mLoggerImpl;
    StreamrLogLevel mLoggerLogLevel;
    nlohmann::json mContextBindings;
    // Cheapest enabled level across this logger AND every
    // LOG_LEVEL_<Category> env override (those can only raise verbosity
    // for a file category). log() early-outs below this bound BEFORE
    // marshalling the metadata to JSON: the marshalling (plus the
    // per-call environment scan in FollyLoggerImpl) ran on every
    // filtered-out trace/debug call and dominated the per-message cost
    // of the connection delivery chains (measured 20-90 ms per network
    // message in Debug builds under the full-node tests).
    int mMinEnabledLogLevelValue{0};

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
    template <typename ContextBindingsType = StreamrJsonInitializerList>
    explicit Logger(
        ContextBindingsType contextBindings = {},
        StreamrLogLevel defaultLogLevel = systemDefaultLogLevel,
        std::shared_ptr<LoggerImpl> loggerImpl = nullptr) {
        // If LOG_LEVEL env variable is set and is valid,
        // use it as the default log level for this logger.
        // Otherwise, use the defaultLogLevel.

        char* val = getenv(envLogLevelName);
        if (val) {
            mLoggerLogLevel = getStreamrLogLevelByName(val, defaultLogLevel);
        } else {
            mLoggerLogLevel = defaultLogLevel;
        }

        mContextBindings =
            ensureJsonObject(toJson(contextBindings), "contextBindings");

        // Snapshot the minimum enabled level once (see the member's
        // comment). Env vars cannot meaningfully change mid-process, and
        // FollyLoggerImpl re-reads them per message anyway for the
        // messages that pass this bound.
        int minEnabled = getStreamrLogLevelValue(mLoggerLogLevel);
        for (char** env = environ; *env != nullptr; ++env) { // NOLINT
            const std::string_view envVar{*env};
            if (envVar.starts_with(detail::envCategoryLogLevelName)) {
                const auto eq = envVar.find('=');
                if (eq != std::string_view::npos) {
                    const auto categoryLevel = getStreamrLogLevelByName(
                        envVar.substr(eq + 1), mLoggerLogLevel);
                    minEnabled = std::min(
                        minEnabled, getStreamrLogLevelValue(categoryLevel));
                }
            }
        }
        mMinEnabledLogLevelValue = minEnabled;

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

    template <typename T = StreamrJsonInitializerList>
    void trace(
        std::string_view msg,
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

    template <typename T = StreamrJsonInitializerList>
    void debug(
        std::string_view msg,
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

    template <typename T = StreamrJsonInitializerList>
    void info(
        std::string_view msg,
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

    template <typename T = StreamrJsonInitializerList>
    void warn(
        std::string_view msg,
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

    template <typename T = StreamrJsonInitializerList>
    void error(
        std::string_view msg,
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

    template <typename T = StreamrJsonInitializerList>
    void fatal(
        std::string_view msg,
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
            (element.is_string() && element.get<std::string>().empty())) {
            return nlohmann::json::object({});
        }
        return nlohmann::json::object({{key, element}});
    }

    // MetadataType can be any type that is convertible to JSON by
    // streamr-json
    template <typename MetadataType>
    void log(
        const StreamrLogLevel messageLogLevel,
        std::string_view msg,
        MetadataType metadata,
        const std::source_location& location) {
        // Early-out below the cheapest enabled level (see
        // mMinEnabledLogLevelValue): no category could emit this message,
        // so skip the metadata marshalling and the LoggerImpl call.
        if (getStreamrLogLevelValue(messageLogLevel) <
            mMinEnabledLogLevelValue) {
            return;
        }
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