#ifndef STREAMR_LOGGER_LOG_LEVEL_MAP_HPP
#define STREAMR_LOGGER_LOG_LEVEL_MAP_HPP

#include <algorithm>
#include <array>
#include <string_view>
#include <variant>
#include <folly/logging/LogLevel.h>

#include "streamr-logger/StreamrLogLevel.hpp"

// A compile-time map between StreamrLogLevel and folly::LogLevel

template <std::size_t Size>
struct LogLevelMap {
    using StreamrLogLevel = streamr::logger::StreamrLogLevel;
    using Mapping = std::pair<StreamrLogLevel, folly::LogLevel>;

    std::array<
        Mapping,
        Size>
        mData; // NOLINT(misc-non-private-member-variables-in-classes)

    [[nodiscard]] constexpr folly::LogLevel streamrLevelToFollyLevel(
        const StreamrLogLevel& key) const {
        const auto itr = std::find_if(
            begin(mData), end(mData), [&key](const Mapping& mapping) {
                return mapping.first.index() == key.index();
            });
        if (itr != end(mData)) {
            return itr->second;
        }
        throw std::range_error("Not Found");
    }

    [[nodiscard]] constexpr folly::LogLevel streamrLevelNameToFollyLevel(
        const std::string_view& name) const {
        const auto itr = std::find_if(
            begin(mData), end(mData), [&name](const Mapping& mapping) {
                return std::visit(
                    [&name](const auto& v) { return v.name == name; },
                    mapping.first);
            });
        if (itr != end(mData)) {
            return itr->second;
        }
        throw std::range_error("Not Found");
    }
};

#endif // STREAMR_LOGGER_LOG_LEVEL_MAP_HPP