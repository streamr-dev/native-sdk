#ifndef STREAMER_LOGGER_STREAMRLOGFORMATTER_H
#define STREAMER_LOGGER_STREAMRLOGFORMATTER_H

#include <folly/Format.h>
#include <folly/logging/LogFormatter.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include "StreamrLogColors.hpp"

namespace streamr::logger {

namespace detail {

static constexpr int MAX_FILENAME_AND_LINE_NUMBER_LENGTH{36};
static constexpr folly::StringPiece FILENAME_AND_LINE_NUMBER_SEPARATOR{": "};
static constexpr auto SEPARATOR_LENGTH{
    std::ssize(FILENAME_AND_LINE_NUMBER_SEPARATOR)};
static const std::string NON_TRUNCATED_FORMATTER =
    "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({:<" +
    std::to_string(MAX_FILENAME_AND_LINE_NUMBER_LENGTH) + "}): {}{}{}\n";
static constexpr std::string_view TRUNCATED_FORMATTER =
    "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({: <*}{}{}): {}{}{}\n";

struct LogLevelData {
    folly::StringPiece logLevelName;
    folly::StringPiece color;
};

// FATAL cannot be used in folly because it aborts. So CRITICAL is converted
// to Streamr fatal
constexpr LogLevelData getLogLevelData(const folly::LogLevel level) {
    switch (level) {
        case folly::LogLevel::DBG:
            return {"TRACE", detail::colors::GRAY};
            break;
        case folly::LogLevel::DBG0:
            return {"DEBUG", detail::colors::BLUE};
            break;
        case folly::LogLevel::INFO:
            return {"INFO", detail::colors::GREEN};
            break;
        case folly::LogLevel::WARN:
            return {"WARN", detail::colors::YELLOW};
            break;
        case folly::LogLevel::ERR:
            return {"ERROR", detail::colors::RED};
            break;
        case folly::LogLevel::CRITICAL:
            return {"FATAL", detail::colors::BG_RED};
            break;
        default:
            return {"TRACE", detail::colors::GRAY};
            break;
    }
}

} // namespace detail

/*
Format based on this Streamr log. The Filename and line number is fixed size
with 36 characters. If it is longer then it is truncated
DEBUG [2024-06-06T13:52:49.816] (<Filename>: <Line Number>     ):<Message>

Examples of a truncated filename
ERROR [2024-06-06T13:52:49.816] (12345678901234567890123456789012: 10):<Message>
ERROR [2024-06-06T13:52:49.816] (1234567890123456789012345678901: 101):<Message>

This one is not truncated:
ERROR [2024-06-06T13:52:49.816] (1234567890123456.cpp: 101           ):<Message>

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

    struct StreamrLogMessage {
        std::chrono::system_clock::time_point timestamp;
        folly::StringPiece fileBasename;
        unsigned int lineNumber;
        folly::LogLevel logLevel;
        const std::string& logMessage;
    };

    std::string formatMessageInStreamrStyle(const StreamrLogMessage& message) {
        struct tm ltime;
        const auto timeSinceEpoch = message.timestamp.time_since_epoch();
        const auto epochSeconds =
            std::chrono::duration_cast<std::chrono::seconds>(timeSinceEpoch);
        const std::chrono::milliseconds millisecs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                timeSinceEpoch) -
            epochSeconds;
        time_t unixTimestamp = epochSeconds.count();
        if (!localtime_r(&unixTimestamp, &ltime)) {
            memset(&ltime, 0, sizeof(ltime));
        }
        auto basename = message.fileBasename;
        const auto fileNameLength = std::ssize(basename);
        const auto lineNumberInString = std::to_string(message.lineNumber);
        const auto lineNumberLength = std::ssize(lineNumberInString);
        //  const std::string logMessageColor = "\033[36m";
        //  const std::string logMessageColorReset = "\033[0m";
        const auto fileNameAndLineNumberLength =
            (fileNameLength + lineNumberLength + detail::SEPARATOR_LENGTH);
        auto logLevelData = detail::getLogLevelData(message.logLevel);
        const auto tmStartYear{1900};
        if (fileNameAndLineNumberLength <=
            detail::MAX_FILENAME_AND_LINE_NUMBER_LENGTH) {
            basename = basename.toString()
                           .append(detail::FILENAME_AND_LINE_NUMBER_SEPARATOR)
                           .append(lineNumberInString);
            auto logLine = folly::sformat(
                detail::NON_TRUNCATED_FORMATTER,
                logLevelData.color,
                logLevelData.logLevelName,
                detail::colors::RESET_COLOR,
                ltime.tm_year + tmStartYear,
                ltime.tm_mon + 1,
                ltime.tm_mday,
                ltime.tm_hour,
                ltime.tm_min,
                ltime.tm_sec,
                millisecs.count(),
                basename,
                detail::colors::CYAN,
                message.logMessage,
                detail::colors::RESET_COLOR);
            return logLine;
        }
        // Truncate needed
        auto lengthForTruncatedFileName =
            detail::MAX_FILENAME_AND_LINE_NUMBER_LENGTH -
            (lineNumberLength + detail::SEPARATOR_LENGTH);
        basename = basename.substr(0, lengthForTruncatedFileName);
        auto logLine = folly::sformat(
            detail::TRUNCATED_FORMATTER,
            logLevelData.color,
            logLevelData.logLevelName,
            detail::colors::RESET_COLOR,
            ltime.tm_year + tmStartYear,
            ltime.tm_mon + 1,
            ltime.tm_mday,
            ltime.tm_hour,
            ltime.tm_min,
            ltime.tm_sec,
            millisecs.count(),
            lengthForTruncatedFileName,
            basename,
            detail::FILENAME_AND_LINE_NUMBER_SEPARATOR,
            lineNumberInString,
            detail::colors::CYAN,
            message.logMessage,
            detail::colors::RESET_COLOR);
        return logLine;
    }

    std::string formatMessage(
        const folly::LogMessage& message,
        const folly::LogCategory* /* handlerCategory */) override {
        return this->formatMessageInStreamrStyle(
            {message.getTimestamp(),
             message.getFileBaseName(),
             message.getLineNumber(),
             message.getLevel(),
             message.getMessage()});
    }
};
}; // namespace streamr::logger

#endif