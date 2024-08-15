#ifndef STREAMR_LOGGER_LOGGERIMPL_HPP
#define STREAMR_LOGGER_LOGGERIMPL_HPP

#include <source_location>
#include <string>

#include "StreamrLogLevel.hpp"

namespace streamr::logger {

// Interface for the logger implementations

class LoggerImpl {
public:
    virtual ~LoggerImpl() = default;

    virtual void init(StreamrLogLevel defaultLogLevel) = 0;

    virtual void sendLogMessage(
        StreamrLogLevel logLevel,
        const std::string& msg,
        const std::string& metadata,
        const std::source_location& location) = 0;
};

} // namespace streamr::logger

#endif // STREAMR_LOGGER_LOGGERIMPL_HPP