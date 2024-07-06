#ifndef STREAMR_LOGGER_STREAMR_LOG_LEVEL_HPP
#define STREAMR_LOGGER_STREAMR_LOG_LEVEL_HPP

#include <string_view>
#include <variant>

#include "detail/StreamrLogColors.hpp"

namespace streamr::logger {

namespace streamrloglevel {

namespace colors = streamr::logger::detail::colors;
struct Trace {
    static constexpr std::string_view name = "trace";
    static constexpr int value = 0;
    static constexpr std::string_view displayName = "TRACE";
    static constexpr std::string_view color = colors::gray;
};
struct Debug {
    static constexpr std::string_view name = "debug";
    static constexpr int value = 1;
    static constexpr std::string_view displayName = "DEBUG";
    static constexpr std::string_view color = colors::blue;
};
struct Info {
    static constexpr std::string_view name = "info";
    static constexpr int value = 2;
    static constexpr std::string_view displayName = "INFO";
    static constexpr std::string_view color = colors::green;
};
struct Warn {
    static constexpr std::string_view name = "warn";
    static constexpr int value = 3;
    static constexpr std::string_view displayName = "WARN";
    static constexpr std::string_view color = colors::yellow;
};
struct Error {
    static constexpr std::string_view name = "error";
    static constexpr int value = 4;
    static constexpr std::string_view displayName = "ERROR";
    static constexpr std::string_view color = colors::red;
};
struct Fatal {
    static constexpr std::string_view name = "fatal";
    static constexpr int value = 5;
    static constexpr std::string_view displayName = "FATAL";
    static constexpr std::string_view color = colors::bgRed;
};
} // namespace streamrloglevel

using StreamrLogLevel = std::variant<
    streamrloglevel::Trace,
    streamrloglevel::Debug,
    streamrloglevel::Info,
    streamrloglevel::Warn,
    streamrloglevel::Error,
    streamrloglevel::Fatal>;

constexpr StreamrLogLevel systemDefaultLogLevel = streamrloglevel::Info{};
template <typename T>
concept StreamrLogLevelConcept = std::is_same_v<T, StreamrLogLevel>;

// Disable default specialization
template <typename... LogLevels>
struct LevelGetter;

// Specialization for std::variant<LogLevels...>
template <typename... LogLevels>
struct LevelGetter<std::variant<LogLevels...>> {
    [[nodiscard]] StreamrLogLevel getStreamrLogLevelByName(
        std::string_view levelName, StreamrLogLevel defaultLogLevel) {
        StreamrLogLevel result = defaultLogLevel;
        ((LogLevels::name == levelName ? result = LogLevels{} : result), ...);
        return result;
    }
};

template <typename T = StreamrLogLevel>
[[nodiscard]] StreamrLogLevel getStreamrLogLevelByName(
    std::string_view levelName, StreamrLogLevel defaultLogLevel) {
    return LevelGetter<T>().getStreamrLogLevelByName(
        levelName, defaultLogLevel);
}

template <StreamrLogLevelConcept T = StreamrLogLevel>
[[nodiscard]] int getStreamrLogLevelValue(T level) {
    return std::visit([](const auto& level) { return level.value; }, level);
}

} // namespace streamr::logger

#endif // STREAMR_LOGGER_STREAMR_LOG_LEVEL_HPP