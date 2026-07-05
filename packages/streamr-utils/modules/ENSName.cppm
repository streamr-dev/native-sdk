// Module streamr.utils.ENSName
// CONSOLIDATED from the former header
// streamr-utils/ENSName.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

export module streamr.utils.ENSName;

import streamr.utils.Branded;

export namespace streamr::utils {

inline bool isENSNameFormatIgnoreCase(std::string_view str) {
    return str.contains('.');
}

using ENSName = Branded<std::string, struct ENSNameBrand>;

inline ENSName toENSName(std::string_view str) {
    if (isENSNameFormatIgnoreCase(str)) {
        auto lowercaseStr = std::string(str);
        std::ranges::transform(
            lowercaseStr, lowercaseStr.begin(), [](unsigned char c) {
                return std::tolower(c);
            });
        return ENSName{std::move(lowercaseStr)};
    }
    throw std::runtime_error("not a valid ENS name: " + std::string(str));
}
} // namespace streamr::utils