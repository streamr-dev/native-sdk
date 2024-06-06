#ifndef STREAMER_UTILS_LOGGER_LOGGER_H
#define STREAMER_UTILS_LOGGER_LOGGER_H
// NOLINTBEGIN(readability-simplify-boolean-expr)

#include "folly/logging/xlog.h"
#include "folly/logging/FileHandlerFactory.h"
#include "folly/logging/StreamHandlerFactory.h"
#include "folly/logging/LoggerDB.h"
#include "folly/logging/LogConfigParser.h"
#include "folly/logging/LogHandlerConfig.h"
#include "folly/logging/LogCategoryConfig.h"
#include "folly/logging/LogConfig.h"
#include "folly/logging/LogFormatter.h"
#include "folly/logging/LogHandler.h"
#include "folly/logging/LogMessage.h"
#include "folly/logging/LogCategory.h"
#include "folly/logging/LogWriter.h"
#include "folly/logging/StandardLogHandlerFactory.h"
#include "folly/logging/StandardLogHandler.h"
#include <memory>
#include <iostream>


class StreamrFormatter : public folly::LogFormatter {

    public:
        StreamrFormatter() = default;
        ~StreamrFormatter() override = default;

    std::string formatMessage(
      const folly::LogMessage& message, const folly::LogCategory* handlerCategory) override {

        return message.getMessage();
    }

};


class StreamrFormatterFactory
    :  public folly::StandardLogHandlerFactory::FormatterFactory {
 public:

  bool processOption(
      folly::StringPiece name, folly::StringPiece value) override {
      }

  std::shared_ptr<folly::LogFormatter> createFormatter(
    const std::shared_ptr<folly::LogWriter>&) override {

    return std::make_shared<StreamrFormatter>();;
}


};

class StreamrHandlerFactory: public folly::StreamHandlerFactory {

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

  Logger() {

    this->initializeLoggerDB(folly::LoggerDB::get());

  }

  void initializeLoggerDB(folly::LoggerDB& db) const {

    db.registerHandlerFactory(std::make_unique<StreamrHandlerFactory>(), true);
    auto defaultHandlerConfig =
      folly::LogHandlerConfig("stream", {{"stream", "stderr"}, {"async", "false"}});
    auto rootCategoryConfig =
      folly::LogCategoryConfig(folly::kDefaultLogLevel, false, {"default"});
    folly::LogConfig config(
       {{"default", defaultHandlerConfig}},
       {{"", rootCategoryConfig}});

    db.updateConfig(config);
  }

  void log(const std::string& message) const{ 
      XLOG(INFO) << "dddkdkdkkdkdkdkdkdkdk"; 
   }
};
#endif
// NOLINTEND(readability-simplify-boolean-expr)
