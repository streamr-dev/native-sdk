#ifndef STREAMER_LOGGER_STREAMRLOGFORMATTER_H
#define STREAMER_LOGGER_STREAMRLOGFORMATTER_H

#include <iostream>
#include <folly/Format.h>
#include <folly/logging/LogFormatter.h>
#include <folly/logging/LogLevel.h>

namespace {

// If you change maxFileNameAndLineNumberLength, please remember to change
// hardcoded 36 in sformat. It can be changed later when knowing how to add 36
// as constant to sformat.
static constexpr int maxFileNameAndLineNumberLength{36};
static constexpr folly::StringPiece fileNameAndLineNumberSeparator{": "};
static constexpr auto separatorLength{
    std::ssize(fileNameAndLineNumberSeparator)};

constexpr folly::StringPiece getLogLevelName(folly::LogLevel level) {
    if (level == folly::LogLevel::DBG) {
        return "TRACE";
    } else if (level == folly::LogLevel::DBG0) {
        return "DEBUG";
    } else if (level == folly::LogLevel::INFO) {
        return "INFO";
    } else if (level == folly::LogLevel::WARN) {
        return "WARN";
    } else if (level == folly::LogLevel::ERR) {
        return "ERROR";
    }
    return "FATAL";
}

folly::StringPiece getColorSequence(folly::LogLevel level) {
    if (level == folly::LogLevel::DBG) {
        return "\033[90m"; // Gray (TRACE)
    } else if (level == folly::LogLevel::DBG0) {
        return "\033[34m"; // Blue (DEBUG)
    } else if (level == folly::LogLevel::INFO) {
        return "\033[32m"; // Green (INFO)
    } else if (level == folly::LogLevel::WARN) {
        return "\033[33m"; // Yellow (WARN)
    } else if (level == folly::LogLevel::ERR) {
        return "\033[31m"; // Red (ERROR)
    }
    return "\033[1;41m"; // Red Background (FATAL)
}

} // namespace

namespace streamr::logger {

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
        const std::string logMessageColor = "\033[36m";
        const std::string logMessageColorReset = "\033[0m";
        const auto fileNameAndLineNumberLength =
            (fileNameLength + lineNumberLength + separatorLength);
        // Is filename is truncated if filename, separator and lineNumber does
        // not fit to the fixed size
        if (fileNameAndLineNumberLength <= maxFileNameAndLineNumberLength) {
            basename = basename.toString()
                           .append(fileNameAndLineNumberSeparator)
                           .append(lineNumberInString);
            auto logLine = folly::sformat(
                "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({:<36}): {}{}{}\n",
                getColorSequence(message.logLevel),
                getLogLevelName(message.logLevel),
                logMessageColorReset,
                ltime.tm_year + 1900,
                ltime.tm_mon + 1,
                ltime.tm_mday,
                ltime.tm_hour,
                ltime.tm_min,
                ltime.tm_sec,
                millisecs.count(),
                basename,
                logMessageColor,
                message.logMessage,
                logMessageColorReset);
            return logLine;
        } else {
            // Truncate needed
            auto lengthForTruncatedFileName = maxFileNameAndLineNumberLength -
                (lineNumberLength + separatorLength);
            basename = basename.substr(0, lengthForTruncatedFileName);
            auto logLine = folly::sformat(
                "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({: <*}{}{}): {}{}{}\n",
                getColorSequence(message.logLevel),
                getLogLevelName(message.logLevel),
                logMessageColorReset,
                ltime.tm_year + 1900,
                ltime.tm_mon + 1,
                ltime.tm_mday,
                ltime.tm_hour,
                ltime.tm_min,
                ltime.tm_sec,
                millisecs.count(),
                lengthForTruncatedFileName,
                basename,
                fileNameAndLineNumberSeparator,
                lineNumberInString,
                logMessageColor,
                message.logMessage,
                logMessageColorReset);
            return logLine;
        }
    }

    std::string formatMessage(
        const folly::LogMessage& message,
        const folly::LogCategory* handlerCategory) override {
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