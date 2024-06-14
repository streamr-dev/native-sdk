#ifndef STREAMER_LOGGER_COLORS_H
#define STREAMER_LOGGER_COLORS_H

namespace streamr::logger::detail::colors {

static const auto GRAY = "\033[90m";
static const auto BLUE = "\033[34m";
static const auto CYAN = "\033[36m";
static const auto YELLOW = "\033[33m";
static const auto GREEN = "\033[32m";
static const auto RED = "\033[31m";
static const auto BG_RED = "\033[1;41m";
static const auto RESET_COLOR = "\033[0m";

}; // namespace streamr::logger::detail::colors

#endif