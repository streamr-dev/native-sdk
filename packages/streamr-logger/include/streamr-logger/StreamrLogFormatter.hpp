#ifndef STREAMER_LOGGER_STREAMRLOGFORMATTER_H
#define STREAMER_LOGGER_STREAMRLOGFORMATTER_H

#include <iostream>
#include <folly/Format.h>
#include <folly/logging/LogFormatter.h>
#include <folly/logging/LogLevel.h>

namespace {

static constexpr int maxFileNameAndLineNumberLength{36};
static constexpr folly::StringPiece fileNameAndLineNumberSeparator{": "};
static constexpr auto separatorLength{
    std::ssize(fileNameAndLineNumberSeparator)};
static const std::string nonTruncatedFormatter =
    "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({:<" +
    std::to_string(maxFileNameAndLineNumberLength) + "}): {}{}{}\n";
static constexpr std::string_view truncatedFormatter =
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
            return {"TRACE", "\033[90m"};
            break;
        case folly::LogLevel::DBG0:
            return {"DEBUG", "\033[34m"};
            break;
        case folly::LogLevel::INFO:
            return {"INFO", "\033[32m"};
            break;
        case folly::LogLevel::WARN:
            return {"WARN", "\033[33m"};
            break;
        case folly::LogLevel::ERR:
            return {"ERROR", "\033[31m"};
            break;
        case folly::LogLevel::CRITICAL:
            return {"FATAL", "\033[1;41m"};
            break;
        default:
            return {"TRACE", "\033[90m"};
            break;
    }
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
        //  string_from<signed, maxFileNameAndLineNumberLength>::value
        auto logLevelData = getLogLevelData(message.logLevel);
        //  constexpr std::string_view firstPartFormatterString = "{}{}{}
        //  [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({:<"; constexpr
        //  std::string_view lastPartFormatterString =  "}): {}{}{}\n";
        //  constexpr std::string_view formatterString =
        //  firstPartFormatterString + lastPartFormatterString;

        // firstPargFormatterString + string_from<signed,
        // maxFileNameAndLineNumberLength>::value

        if (fileNameAndLineNumberLength <= maxFileNameAndLineNumberLength) {
            //   auto testi = string_from<signed, -1>::value;
            basename = basename.toString()
                           .append(fileNameAndLineNumberSeparator)
                           .append(lineNumberInString);
            auto logLine = folly::sformat(
                nonTruncatedFormatter,
                logLevelData.color,
                logLevelData.logLevelName,
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
                truncatedFormatter,
                //  "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({:
                //  <*}{}{}): {}{}{}\n",
                logLevelData.color,
                logLevelData.logLevelName,
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