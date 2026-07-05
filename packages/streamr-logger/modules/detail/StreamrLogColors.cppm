// Module streamr.logger.StreamrLogColors
// CONSOLIDATED from the former header
// streamr-logger/detail/StreamrLogColors.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <string>
#include <string_view>

export module streamr.logger.StreamrLogColors;

export namespace streamr::logger::detail::colors {

inline constexpr std::string_view gray = "\033[90m";
inline constexpr std::string_view blue = "\033[34m";
inline constexpr std::string_view cyan = "\033[36m";
inline constexpr std::string_view yellow = "\033[33m";
inline constexpr std::string_view green = "\033[32m";
inline constexpr std::string_view red = "\033[31m";
inline constexpr std::string_view bgRed = "\033[1;41m";
inline constexpr std::string_view resetColor = "\033[0m";

}; // namespace streamr::logger::detail::colors