#ifndef STREAMER_LOGGER_COLORS_H
#define STREAMER_LOGGER_COLORS_H

#include <string_view>

namespace streamr::logger::detail::colors {

static constexpr std::string_view gray = "\033[90m";
static constexpr std::string_view blue = "\033[34m";
static constexpr std::string_view cyan = "\033[36m";
static constexpr std::string_view yellow = "\033[33m";
static constexpr std::string_view green = "\033[32m";
static constexpr std::string_view red = "\033[31m";
static constexpr std::string_view bgRed = "\033[1;41m";
static constexpr std::string_view resetColor = "\033[0m";

}; // namespace streamr::logger::detail::colors

#endif