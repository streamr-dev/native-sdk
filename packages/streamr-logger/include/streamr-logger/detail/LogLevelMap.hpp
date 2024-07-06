#ifndef STREAMR_LOGGER_LOG_LEVEL_MAP_HPP
#define STREAMR_LOGGER_LOG_LEVEL_MAP_HPP

#include <algorithm>
#include <array>
#include <string_view>
#include <variant>
#include <folly/logging/LogLevel.h>

#include "streamr-logger/StreamrLogLevel.hpp"

namespace streamr::logger::detail {

constexpr folly::LogLevel defaultFollyLogLevel = folly::LogLevel::INFO;
constexpr StreamrLogLevel defaultStreamrLogLevel =
    streamr::logger::systemDefaultLogLevel;

// A compile-time map between StreamrLogLevel and folly::LogLevel

struct LogLevelMap {
    using StreamrLogLevel = streamr::logger::StreamrLogLevel;
    using Mapping = std::pair<StreamrLogLevel, folly::LogLevel>;

    static constexpr size_t logLevelMapSize = 6;
    std::array<
        Mapping,
        logLevelMapSize>
        mData; // NOLINT (misc-non-private-member-variables-in-classes)

    // You can change the log level mapping
    // between Streamr and folly by editing
    // this magic static map

    static const LogLevelMap& instance() {
        static constexpr LogLevelMap logLevelMap{
            {{{streamrloglevel::Trace{}, folly::LogLevel::DBG},
              {streamrloglevel::Debug{}, folly::LogLevel::DBG0},
              {streamrloglevel::Info{}, folly::LogLevel::INFO},
              {streamrloglevel::Warn{}, folly::LogLevel::WARN},
              {streamrloglevel::Error{}, folly::LogLevel::ERR},
              {streamrloglevel::Fatal{}, folly::LogLevel::CRITICAL}}}};

        return logLevelMap;
    }

    [[nodiscard]] constexpr folly::LogLevel streamrLevelToFollyLevel(
        const StreamrLogLevel& key) const {
        const auto* const itr = std::find_if(
            begin(mData), end(mData), [&key](const Mapping& mapping) {
                return mapping.first.index() == key.index();
            });
        if (itr != end(mData)) {
            return itr->second;
        }
        return defaultFollyLogLevel;
    }

    [[nodiscard]] constexpr StreamrLogLevel follyLevelToStreamrLevel(
        const folly::LogLevel& key) const {
        const auto* const itr = std::find_if(
            begin(mData), end(mData), [&key](const Mapping& mapping) {
                return mapping.second == key;
            });
        if (itr != end(mData)) {
            return itr->first;
        }
        return defaultStreamrLogLevel;
    }

    [[nodiscard]] constexpr folly::LogLevel streamrLevelNameToFollyLevel(
        const std::string_view& name) const {
        const auto* const itr = std::find_if(
            begin(mData), end(mData), [&name](const Mapping& mapping) {
                return std::visit(
                    [&name](const auto& v) { return v.name == name; },
                    mapping.first);
            });
        if (itr != end(mData)) {
            return itr->second;
        }
        return defaultFollyLogLevel;
    }
};

} // namespace streamr::logger::detail

#endif // STREAMR_LOGGER_LOG_LEVEL_MAP_HPP