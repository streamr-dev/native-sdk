#ifndef STREAMER_LOGGER_SLOGGER_HPP
#define STREAMER_LOGGER_SLOGGER_HPP

#include <source_location>
#include <string_view>
#include <nlohmann/json.hpp>
#include "streamr-logger/Logger.hpp"

namespace streamr::logger {

using streamr::json::StreamrJsonInitializerList;
class SLogger {
public:
    /**
     * @brief Log a message at the trace level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = StreamrJsonInitializerList>
    static void trace(
        std::string_view msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        Logger::instance().trace(msg, metadata, location);
    }

    /**
     * @brief Log a message at the debug level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = StreamrJsonInitializerList>
    static void debug(
        std::string_view msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        Logger::instance().debug(msg, metadata, location);
    }

    /**
     * @brief Log a message at the info level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = StreamrJsonInitializerList>
    static void info(
        std::string_view msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        Logger::instance().info(msg, metadata, location);
    }

    /**
     * @brief Log a message at the warn level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = StreamrJsonInitializerList>
    static void warn(
        std::string_view msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        Logger::instance().warn(msg, metadata, location);
    }

    /**
     * @brief Log a message at the error level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = StreamrJsonInitializerList>
    static void error(
        std::string_view msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        Logger::instance().error(msg, metadata, location);
    }

    /**
     * @brief Log a message at the fatal level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = StreamrJsonInitializerList>
    static void fatal(
        std::string_view msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        Logger::instance().fatal(msg, metadata, location);
    }
};
}; // namespace streamr::logger

#endif
