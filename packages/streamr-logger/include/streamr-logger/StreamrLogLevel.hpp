#ifndef STREAMR_LOGGER_STREAMR_LOG_LEVEL_HPP
#define STREAMR_LOGGER_STREAMR_LOG_LEVEL_HPP

#include <string_view>
#include <variant>

namespace streamr::logger {

namespace streamrloglevel {
struct Trace {
    static constexpr std::string_view name = "trace";
};
struct Debug {
    static constexpr std::string_view name = "debug";
};
struct Info {
    static constexpr std::string_view name = "info";
};
struct Warn {
    static constexpr std::string_view name = "warn";
};
struct Error {
    static constexpr std::string_view name = "error";
};
struct Fatal {
    static constexpr std::string_view name = "fatal";
};
} // namespace streamrloglevel

using StreamrLogLevel = std::variant<
    streamrloglevel::Trace,
    streamrloglevel::Debug,
    streamrloglevel::Info,
    streamrloglevel::Warn,
    streamrloglevel::Error,
    streamrloglevel::Fatal>;

} // namespace streamr::logger

#endif // STREAMR_LOGGER_STREAMR_LOG_LEVEL_HPP