#ifndef STREAMR_UTILS_ETHEREUM_ADDRESS_HPP
#define STREAMR_UTILS_ETHEREUM_ADDRESS_HPP

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>

#include "streamr-utils/Branded.hpp"

namespace streamr::utils {

using EthereumAddress = Branded<std::string, struct EthereumAddressBrand>;

inline EthereumAddress toEthereumAddress(std::string_view str) {
    // AI-generated validation for regexp ^0x[a-fA-F0-9]{40}$

    if (str.size() == 42 && str[0] == '0' && str[1] == 'x' && // NOLINT
        std::all_of(str.begin() + 2, str.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F');
        })) {
        return EthereumAddress{std::string(str)};
    }
    throw std::runtime_error(
        "not a valid Ethereum address: " + std::string(str));
}

} // namespace streamr::utils

#endif // STREAMR_UTILS_ETHEREUM_ADDRESS_HPP