#ifndef STREAMER_LOGGER_STREAMRLOGFORMATTER_HPP
#define STREAMER_LOGGER_STREAMRLOGFORMATTER_HPP

#include <cstddef>
#include <cstdlib>
#include <string_view>
#include <folly/Format.h>
#include <folly/logging/LogFormatter.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include "LogLevelMap.hpp"
#include "StreamrLogColors.hpp"

namespace streamr::logger::detail {

namespace constants {

constexpr std::string_view logColorsEnvVar = "LOG_COLORS";

// If you change MaxFileNameAndLineNumberLength, then please change it in
// nonTruncatedFormatterPart too
constexpr size_t maxFileNameAndLineNumberLength{36};
constexpr std::string_view fileNameAndLineNumberSeparator = ": ";
constexpr size_t separatorLength{std::ssize(fileNameAndLineNumberSeparator)};
constexpr std::string_view firstPartOfLogMessageFormatter =
    "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] (";
constexpr std::string_view nonTruncatedFormatterPart = "{:<36}";
constexpr std::string_view truncatedFormatterPart = "{: <*}{}{}";
constexpr std::string_view lastPartOfLogMessageFormatter = "): {}{}{}";

} // namespace constants

/*
Format based on this Streamr log. The Filename and line number is fixed size
with 36 characters. If it is longer then it is truncated
DEBUG [2024-06-06T13:52:49.816] (<Filename>: <Line Number>     ):<Message>

Examples of a truncated filename
ERROR [2024-06-06T13:52:49.816] (12345678901234567890123456789012: 10):<Message>
ERROR [2024-06-06T13:52:49.816] (1234567890123456789012345678901: 101):<Message>

This one is not truncated:
ERROR [2024-06-06T13:52:49.816] (1234567890123456.cpp: 101           ):<Message>

Also Context bindings and/or metadata are added after Message if needed for
example ERROR [2024-06-06T13:52:49.816] (1234567890123456789012345678901:
101):<Message>
{\"foo1\":\"bar1A\",\"foo2\":42,\"foo3\":\"bar3\",\"foo4\":24,\"foo5\":\"bar5\"}


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

    std::string formatMessage(
        const folly::LogMessage& message,
        const folly::LogCategory* /* handlerCategory */) override {
        return formatMessageInStreamrStyle(
            {message.getTimestamp(),
             message.getFileBaseName(),
             message.getLineNumber(),
             message.getLevel(),
             message.getMessage()});
    }
    struct LogLevelNameAndColor {
        std::string_view logLevelName;
        std::string_view color;
    };
    struct StreamrLogMessage {
        std::chrono::system_clock::time_point timestamp;
        folly::StringPiece fileBasename;
        unsigned int lineNumber;
        folly::LogLevel logLevel;
        const std::string& logMessage;
    };

    // FATAL cannot be used in folly because it aborts. So CRITICAL is converted
    // to Streamr fatal. This function can be used at compile time.
    static LogLevelNameAndColor getLogLevelNameAndColor(
        const folly::LogLevel level) {
        auto streamrLevel =
            LogLevelMap::instance().follyLevelToStreamrLevel(level);
        std::string_view logLevelName;
        std::string_view color;

        std::visit(
            [&](const auto& arg) {
                logLevelName = arg.displayName;
                color = arg.color;
            },
            streamrLevel);

        return {logLevelName, color};
    }

    static bool getColorsSettingFromEnv() {
        const auto* const env = std::getenv(constants::logColorsEnvVar.data());
        return (env == nullptr || std::string_view(env) != "false");
    }

    static std::string formatMessageInStreamrStyle(
        const StreamrLogMessage& message) {
        struct tm ltime;
        const auto timeSinceEpoch = message.timestamp.time_since_epoch();
        const auto epochSeconds =
            std::chrono::duration_cast<std::chrono::seconds>(timeSinceEpoch);
        const std::chrono::milliseconds millisecs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                timeSinceEpoch) -
            epochSeconds;
        time_t unixTimestamp = epochSeconds.count();
        localtime_r(&unixTimestamp, &ltime);
        auto basename = message.fileBasename.toString();
        const auto fileNameLength = std::ssize(basename);
        const auto lineNumberInString = std::to_string(message.lineNumber);
        const auto lineNumberLength = std::ssize(lineNumberInString);
        const auto fileNameAndLineNumberLength =
            (fileNameLength + lineNumberLength + constants::separatorLength);
        auto logLevelData = getLogLevelNameAndColor(message.logLevel);
        const auto tmStartYear{1900};
        bool colorsOn = getColorsSettingFromEnv();

        auto firstPartOfLogLine = folly::sformat(
            constants::firstPartOfLogMessageFormatter,
            (colorsOn ? logLevelData.color : ""),
            logLevelData.logLevelName,
            (colorsOn ? colors::resetColor : ""),
            ltime.tm_year + tmStartYear,
            ltime.tm_mon + 1,
            ltime.tm_mday,
            ltime.tm_hour,
            ltime.tm_min,
            ltime.tm_sec,
            millisecs.count());
        std::string middlePartOfLogLine;
        if (fileNameAndLineNumberLength <=
            constants::maxFileNameAndLineNumberLength) {
            // No truncate
            basename = basename +
                std::string(constants::fileNameAndLineNumberSeparator) +
                lineNumberInString;
            middlePartOfLogLine =
                folly::sformat(constants::nonTruncatedFormatterPart, basename);
        } else {
            // Truncate needed
            auto lengthForTruncatedFileName =
                constants::maxFileNameAndLineNumberLength -
                (lineNumberLength + constants::separatorLength);
            basename = basename.substr(0, lengthForTruncatedFileName);
            middlePartOfLogLine = folly::sformat(
                constants::truncatedFormatterPart,
                lengthForTruncatedFileName,
                basename,
                constants::fileNameAndLineNumberSeparator,
                lineNumberInString);
        }
        auto lastPartOfLogLine = folly::sformat(
            constants::lastPartOfLogMessageFormatter,
            (colorsOn ? colors::cyan : ""),
            message.logMessage,
            (colorsOn ? colors::resetColor : ""));
        return firstPartOfLogLine + middlePartOfLogLine + lastPartOfLogLine +
            "\n";
    }
};

} // namespace streamr::logger::detail

#endif
