#ifndef STREAMER_UTILS_LOGGER_LOGGER_H
#define STREAMER_UTILS_LOGGER_LOGGER_H
// NOLINTBEGIN(readability-simplify-boolean-expr)

#include <folly/logging/LogCategory.h>
#include <folly/logging/LogFormatter.h>
#include <folly/logging/LogHandler.h>
#include <folly/logging/LogMessage.h>
#include <folly/logging/Logger.h>
#include <folly/logging/StandardLogHandler.h>
#include <folly/logging/StandardLogHandlerFactory.h>
#include <folly/logging/StreamHandlerFactory.h>
#include <folly/logging/xlog.h>

class StreamrFormatter : public folly::LogFormatter {
  std::string formatMessage(
      const folly::LogMessage& message,
      const folly::LogCategory* handlerCategory) override {
    return "jeee!";
  }
};

class StreamrHandlerFactory : public folly::StreamHandlerFactory {
  std::shared_ptr<folly::LogHandler> createHandler(
      const Options& options) override {
    WriterFactory writerFactory;
    std::shared_ptr<folly::StandardLogHandler> temp =
        folly::StandardLogHandlerFactory::createHandler(
            getType(), &writerFactory, options);
    // std::shared_ptr<folly::LogHandler> base(temp, temp.get());
    // folly::LogHandler* basePtr = temp.get();
    // std::shared_ptr<folly::LogHandler> jee =
    // static_pointer_cast<folly::LogHandler>(temp);
  }
};

class Logger {
 private:
  folly::Logger logger;

 public:
  Logger() : logger("omaloggeri") {}
  // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
  void log(const std::string& message) const { XLOG(INFO) << message; }
};

// NOLINTEND(readability-simplify-boolean-expr)
#endif
