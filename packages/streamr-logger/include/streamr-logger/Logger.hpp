#ifndef STREAMER_UTILS_LOGGER_LOGGER_HPP
#define STREAMER_UTILS_LOGGER_LOGGER_HPP

#include <iostream>

namespace streamr::logger {
class Logger {
public:
    Logger() = default;

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    void log(const std::string& message) const {
        std::cout << message << std::endl;
    }
};

} // namespace streamr::logger
#endif
