#ifndef STREAMER_UTILS_LOGGER_LOGGER_H
#define STREAMER_UTILS_LOGGER_LOGGER_H

#include <folly/logging/xlog.h>
class Logger {
private:
    folly::Logger logger;
public:
    Logger() : logger("omaloggeri") {
    }

    void log(const std::string& message) {
        XLOG(INFO) << message;
    }
};

#endif