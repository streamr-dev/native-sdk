#ifndef STREAMR_LOGGER_STREAMR_LOG_LEVEL_HPP
#define STREAMR_LOGGER_STREAMR_LOG_LEVEL_HPP

#include <string_view>
#include <variant>

namespace streamr::logger {

namespace streamrloglevel {
struct Trace {
    static constexpr std::string_view name = "trace";
    static constexpr int value = 0;
};
struct Debug {
    static constexpr std::string_view name = "debug";
    static constexpr int value = 1;
};
struct Info {
    static constexpr std::string_view name = "info";
    static constexpr int value = 2;
};
struct Warn {
    static constexpr std::string_view name = "warn";
    static constexpr int value = 3;
};
struct Error {
    static constexpr std::string_view name = "error";
    static constexpr int value = 4;
};
struct Fatal {
    static constexpr std::string_view name = "fatal";
    static constexpr int value = 5;
};
} // namespace streamrloglevel

using StreamrLogLevel = std::variant<
    streamrloglevel::Trace,
    streamrloglevel::Debug,
    streamrloglevel::Info,
    streamrloglevel::Warn,
    streamrloglevel::Error,
    streamrloglevel::Fatal>;

constexpr StreamrLogLevel defaultLogLevel = streamrloglevel::Info{};

// Disable default specialization
template <typename... LogLevels>
struct LevelGetter;

// Specialization for std::variant<LogLevels...>
template <typename... LogLevels>
struct LevelGetter<std::variant<LogLevels...>> {
    [[nodiscard]] StreamrLogLevel getStreamrLogLevelByName(
        std::string_view levelName) {
        StreamrLogLevel result = defaultLogLevel;
        ((LogLevels::name == levelName ? result = LogLevels{} : result), ...);
        return result;
    }
};

/*
template<typename ...T>
[[nodiscard]] StreamrLogLevel getStreamrLogLevelByNameImpl(std::string_view
levelName) { StreamrLogLevel result = defaultLogLevel;
    ((T::name == levelName ? result = T{} : result), ...);
    return result;
}
*/
template <typename T = StreamrLogLevel>
[[nodiscard]] StreamrLogLevel getStreamrLogLevelByName(
    std::string_view levelName) {
    return LevelGetter<T>().getStreamrLogLevelByName(levelName);
}

} // namespace streamr::logger

#endif // STREAMR_LOGGER_STREAMR_LOG_LEVEL_HPP