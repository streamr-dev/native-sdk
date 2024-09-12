#ifndef STREAMR_UTILS_ENS_NAME_HPP
#define STREAMR_UTILS_ENS_NAME_HPP

#include <algorithm>
#include <string>
#include "streamr-utils/Branded.hpp"
namespace streamr::utils {

inline bool isENSNameFormatIgnoreCase(std::string_view str) {
    return str.find('.') != std::string_view::npos;
}

using ENSName = Branded<std::string, struct ENSNameBrand>;

inline ENSName toENSName(std::string_view str) {
    if (isENSNameFormatIgnoreCase(str)) {
        auto lowercaseStr = std::string(str);
        std::transform(
            lowercaseStr.begin(),
            lowercaseStr.end(),
            lowercaseStr.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return ENSName{std::move(lowercaseStr)};
    }
    throw std::runtime_error("not a valid ENS name: " + std::string(str));
}
} // namespace streamr::utils
#endif // STREAMR_UTILS_ENS_NAME_HPP
