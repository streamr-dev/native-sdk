#ifndef STREAMER_LOGGER_STREAMRLOGFORMATTER_H
#define STREAMER_LOGGER_STREAMRLOGFORMATTER_H

#include <folly/Format.h>
#include <folly/logging/LogFormatter.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include "StreamrLogColors.hpp"

namespace streamr::logger {

namespace detail {

// If you change MaxFileNameAndLineNumberLength, then please change it in
// nonTruncatedFormatter too
static const int MaxFileNameAndLineNumberLength{36};
static const std::string FileNameAndLineNumberSeparator{": "};
static const auto SeparatorLength{std::ssize(FileNameAndLineNumberSeparator)};
static constexpr std::string_view nonTruncatedFormatter =
    "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({:<36}): {}{}{}\n";
static constexpr std::string_view truncatedFormatter =
    "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({: <*}{}{}): {}{}{}\n";
struct LogLevelData {
    std::string_view logLevelName;
    std::string_view color;
};

// FATAL cannot be used in folly because it aborts. So CRITICAL is converted
// to Streamr fatal
constexpr LogLevelData getLogLevelData(const folly::LogLevel level) {
    switch (level) {
        case folly::LogLevel::DBG:
            return {"TRACE", detail::colors::Gray};
            break;
        case folly::LogLevel::DBG0:
            return {"DEBUG", detail::colors::Blue};
            break;
        case folly::LogLevel::INFO:
            return {"INFO", detail::colors::Green};
            break;
        case folly::LogLevel::WARN:
            return {"WARN", detail::colors::Yellow};
            break;
        case folly::LogLevel::ERR:
            return {"ERROR", detail::colors::Red};
            break;
        case folly::LogLevel::CRITICAL:
            return {"FATAL", detail::colors::BgRed};
            break;
        default:
            return {"TRACE", detail::colors::Gray};
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
        auto basename = message.fileBasename.toString();
        const auto fileNameLength = std::ssize(basename);
        const auto lineNumberInString = std::to_string(message.lineNumber);
        const auto lineNumberLength = std::ssize(lineNumberInString);
        const auto fileNameAndLineNumberLength =
            (fileNameLength + lineNumberLength + detail::SeparatorLength);
        auto logLevelData = detail::getLogLevelData(message.logLevel);
        const auto tmStartYear{1900};
        if (fileNameAndLineNumberLength <=
            detail::MaxFileNameAndLineNumberLength) {
            basename = basename + detail::FileNameAndLineNumberSeparator +
                lineNumberInString;
            auto logLine = folly::sformat(
                detail::nonTruncatedFormatter,
                logLevelData.color,
                logLevelData.logLevelName,
                detail::colors::ResetColor,
                ltime.tm_year + tmStartYear,
                ltime.tm_mon + 1,
                ltime.tm_mday,
                ltime.tm_hour,
                ltime.tm_min,
                ltime.tm_sec,
                millisecs.count(),
                basename,
                detail::colors::Cyan,
                message.logMessage,
                detail::colors::ResetColor);
            return logLine;
        }
        // Truncate needed
        auto lengthForTruncatedFileName =
            detail::MaxFileNameAndLineNumberLength -
            (lineNumberLength + detail::SeparatorLength);
        basename = basename.substr(0, lengthForTruncatedFileName);
        auto logLine = folly::sformat(
            detail::truncatedFormatter,
            logLevelData.color,
            logLevelData.logLevelName,
            detail::colors::ResetColor,
            ltime.tm_year + tmStartYear,
            ltime.tm_mon + 1,
            ltime.tm_mday,
            ltime.tm_hour,
            ltime.tm_min,
            ltime.tm_sec,
            millisecs.count(),
            lengthForTruncatedFileName,
            basename,
            detail::FileNameAndLineNumberSeparator,
            lineNumberInString,
            detail::colors::Cyan,
            message.logMessage,
            detail::colors::ResetColor);
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