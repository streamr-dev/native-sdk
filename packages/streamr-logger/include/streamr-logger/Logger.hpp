#ifndef STREAMER_UTILS_LOGGER_LOGGER_H
#define STREAMER_UTILS_LOGGER_LOGGER_H

#include <folly/logging/xlog.h>
class Logger 
 {
private:
    folly::Logger  logger
    ;
public:
    Logger() : logger("omaloggeri") {
    }
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    void log(const std::string& message) const {
        XLOG(INFO) << message;
    }
};

#endif
