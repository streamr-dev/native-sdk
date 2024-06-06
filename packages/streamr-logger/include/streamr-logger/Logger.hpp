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
Format based on this Streamr log:

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
  folly::StringPiece getLogLevelName(folly::LogLevel level) {
    if (level < folly::LogLevel::DBG) {
      return "TRACE";
    } else if (level < folly::LogLevel::INFO) {
      return "DEBUG";
    } else if (level < folly::LogLevel::WARN) {
      return "INFO";
    } else if (level < folly::LogLevel::ERR) {
      return "WARN";
    } else if (level < folly::LogLevel::CRITICAL) {
      return "ERROR";
    }
    return "FATAL";
  }

  std::string formatMessage(
      const folly::LogMessage& message,
      const folly::LogCategory* handlerCategory) override {
   
    struct tm ltime;
    auto timeSinceEpoch = message.getTimestamp().time_since_epoch();
    auto epochSeconds =
        std::chrono::duration_cast<std::chrono::seconds>(timeSinceEpoch);
    std::chrono::microseconds usecs =
        std::chrono::duration_cast<std::chrono::microseconds>(timeSinceEpoch) -
        epochSeconds;
    time_t unixTimestamp = epochSeconds.count();
    if (!localtime_r(&unixTimestamp, &ltime)) {
      memset(&ltime, 0, sizeof(ltime));
    }
    auto basename = message.getFileBaseName();
    auto logLine = folly::sformat(
        "{} [{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}] ({}): {}\n",
        getLogLevelName(message.getLevel()),
        ltime.tm_year + 1900,
        ltime.tm_mon + 1,
        ltime.tm_mday,
        ltime.tm_hour,
        ltime.tm_min,
        ltime.tm_sec,
        basename,
        message.getMessage());
    return logLine;
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

  void log(const std::string& message) const {
    XLOG(INFO) << "dddkdkdkkdkdkdkdkdkdk";
  }
};
#endif
// NOLINTEND(readability-simplify-boolean-expr)
