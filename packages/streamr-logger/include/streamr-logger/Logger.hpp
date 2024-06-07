#ifndef STREAMER_UTILS_LOGGER_LOGGER_H
#define STREAMER_UTILS_LOGGER_LOGGER_H
// NOLINTBEGIN(readability-simplify-boolean-expr)

#include <folly/Format.h>
#include <folly/logging/FileHandlerFactory.h>
#include <folly/logging/LogCategory.h>
#include <folly/logging/LogCategoryConfig.h>
#include <folly/logging/LogConfig.h>
#include <folly/logging/LogConfigParser.h>
#include <folly/logging/LogFormatter.h>
#include <folly/logging/LogHandler.h>
#include <folly/logging/LogHandlerConfig.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/LogMessage.h>
#include <folly/logging/LogWriter.h>
#include <folly/logging/LoggerDB.h>
#include <folly/logging/StandardLogHandler.h>
#include <folly/logging/StandardLogHandlerFactory.h>
#include <folly/logging/StreamHandlerFactory.h>
#include <folly/logging/xlog.h>
#include <folly/portability/Time.h>

#include <iostream>
#include <memory>

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

class StreamrFormatter : public folly::LogFormatter {
 public:
  StreamrFormatter() = default;
  ~StreamrFormatter() override = default;

 private:
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

  constexpr folly::StringPiece getResetSequence(folly::LogLevel level) {
    return "\033[0m";
  }

  folly::StringPiece getColorSequence(folly::LogLevel level) {
    if (level == folly::LogLevel::DBG) {
      return "\033[1;30m";
    } else if (level == folly::LogLevel::DBG0) {
      return "\033[33m"; // YELLOW COLOR
    } else if (level == folly::LogLevel::INFO) {
      return "\033[33m"; // YELLOW COLOR
    } else if (level == folly::LogLevel::WARN) {
      return "\033[31m"; // RED
    } else if (level == folly::LogLevel::ERR) {
      return "\033[1;41m"; // BOLD ON RED BACKGROUND
    }
    return "\033[1;41m"; // BOLD ON RED BACKGROUND
  }

  std::string formatMessage(
      const folly::LogMessage& message,
      const folly::LogCategory* handlerCategory) override {
    struct tm ltime;
    const auto timeSinceEpoch = message.getTimestamp().time_since_epoch();
    const auto epochSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(timeSinceEpoch);
    const std::chrono::milliseconds millisecs =
        std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceEpoch) -
        epochSeconds;
    time_t unixTimestamp = epochSeconds.count();
    if (!localtime_r(&unixTimestamp, &ltime)) {
      memset(&ltime, 0, sizeof(ltime));
    }
    auto basename = message.getFileBaseName();
    const auto fileNameLength = std::ssize(basename);
    const auto lineNumberInString = std::to_string(message.getLineNumber());
    const auto lineNumberLength = std::ssize(lineNumberInString);

    const auto fileNameAndLineNumberLength =
        (fileNameLength + lineNumberLength + separatorLength);
    // Is filename is truncated if filename, separator and lineNumber does not
    // fit to the fixed size
    if (fileNameAndLineNumberLength <= maxFileNameAndLineNumberLength) {
      basename = basename.toString()
                     .append(fileNameAndLineNumberSeparator)
                     .append(lineNumberInString);
      auto logLine = folly::sformat(
          "{}{}{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({:<36}): {}\n",
          getColorSequence(message.getLevel()),
          getLogLevelName(message.getLevel()),
          getResetSequence(message.getLevel()),
          ltime.tm_year + 1900,
          ltime.tm_mon + 1,
          ltime.tm_mday,
          ltime.tm_hour,
          ltime.tm_min,
          ltime.tm_sec,
          millisecs.count(),
          basename,
          message.getMessage());
      return logLine;
    } else {
      // Truncate needed
      auto lengthForTruncatedFileName =
          maxFileNameAndLineNumberLength - (lineNumberLength + separatorLength);
      basename = basename.substr(0, lengthForTruncatedFileName);
      auto logLine = folly::sformat(
          "{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{}] ({: <*}{}{}): {}\n",
          getLogLevelName(message.getLevel()),
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
          message.getMessage());
      return logLine;
    }
  }
};

class StreamrFormatterFactory
    : public folly::StandardLogHandlerFactory::FormatterFactory {
 public:
  bool processOption(
      folly::StringPiece name, folly::StringPiece value) override {
    return false;
  }

  std::shared_ptr<folly::LogFormatter> createFormatter(
      const std::shared_ptr<folly::LogWriter>&) override {
    return std::make_shared<StreamrFormatter>();
    ;
  }
};

class StreamrHandlerFactory : public folly::StreamHandlerFactory {
 public:
  StreamrHandlerFactory() = default;
  ~StreamrHandlerFactory() override = default;

  std::shared_ptr<folly::LogHandler> createHandler(
      const Options& options) override {
    StreamHandlerFactory::WriterFactory writerFactory;
    StreamrFormatterFactory formatterFactory;

    return folly::StandardLogHandlerFactory::createHandler(
        getType(), &writerFactory, &formatterFactory, options);
  }
};

class Logger {

 public:
  Logger() { this->initializeLoggerDB(folly::LoggerDB::get()); }

  void initializeLoggerDB(folly::LoggerDB& db) const {
    db.registerHandlerFactory(std::make_unique<StreamrHandlerFactory>(), true);
    auto defaultHandlerConfig = folly::LogHandlerConfig(
        "stream", {{"stream", "stderr"}, {"async", "false"}});
    auto rootCategoryConfig =
        folly::LogCategoryConfig(folly::kDefaultLogLevel, false, {"default"});
    folly::LogConfig config(
        {{"default", defaultHandlerConfig}}, {{"", rootCategoryConfig}});
    db.updateConfig(config);
  }

  void log(const std::string& message) const {XLOG(INFO) << message;}
  void logTrace(const std::string& message) const { XLOG(DBG) << message;}
  void logDebug(const std::string& message) const { XLOG(DBG0) << message;}
  void logInfo(const std::string& message) const { XLOG(INFO) << message;}
  void logWarn(const std::string& message) const { XLOG(WARN) << message;}
  void logError(const std::string& message) const { XLOG(ERR) << message;}
  void logFatal(const std::string& message) const { XLOG(FATAL) << message;}
};
#endif
// NOLINTEND(readability-simplify-boolean-expr)
