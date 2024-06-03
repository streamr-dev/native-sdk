#ifndef STREAMER_UTILS_LOGGER_LOGGER_H
#define STREAMER_UTILS_LOGGER_LOGGER_H

#include <folly/logging/xlog.h>
class Logger {
private:
    folly::Logger logger
    ;
    int Iso_jee;
public:
    Logger() : logger("omaloggeri") {
    }

    void log(const std::string& message) const {
        XLOG(INFO)<<message;
        auto Jee=1;
    }
};

#endif