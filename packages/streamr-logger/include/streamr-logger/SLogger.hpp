#ifndef STREAMER_LOGGER_SLOGGER_HPP
#define STREAMER_LOGGER_SLOGGER_HPP

#include <initializer_list>
#include <source_location>
#include <nlohmann/json.hpp>
#include "streamr-logger/Logger.hpp"

namespace streamr::logger {

class SLogger {
public:
    /**
     * @brief Log a message at the trace level.
     * @param msg Message to log.
     * @param metadata (any type except classes/structs with private sections)
     * Metadata to add to the log message.
     */

    template <typename T = std::initializer_list<JsonBuilder>>
    static void trace(
        const char* msg,
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

    template <typename T = std::initializer_list<JsonBuilder>>
    static void debug(
        const char* msg,
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

    template <typename T = std::initializer_list<JsonBuilder>>
    static void info(
        const char* msg,
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

    template <typename T = std::initializer_list<JsonBuilder>>
    static void warn(
        const char* msg,
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

    template <typename T = std::initializer_list<JsonBuilder>>
    static void error(
        const char* msg,
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

    template <typename T = std::initializer_list<JsonBuilder>>
    static void fatal(
        const char* msg,
        T metadata = {},
        const std::source_location& location =
            std::source_location::current()) {
        Logger::instance().fatal(msg, metadata, location);
    }
};
}; // namespace streamr::logger

#endif
