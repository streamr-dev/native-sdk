#ifndef STREAMER_LOGGER_COLORS_H
#define STREAMER_LOGGER_COLORS_H

namespace streamr::logger::detail::colors {

static const auto Gray = "\033[90m";
static const auto Blue = "\033[34m";
static const auto Cyan = "\033[36m";
static const auto Yellow = "\033[33m";
static const auto Green = "\033[32m";
static const auto Red = "\033[31m";
static const auto BgRed = "\033[1;41m";
static const auto ResetColor = "\033[0m";

}; // namespace streamr::logger::detail::colors

#endif