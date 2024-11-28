#ifndef STREAMR_UTILS_TO_ETHEREUM_ADDRESS_OR_ENS_NAME_HPP
#define STREAMR_UTILS_TO_ETHEREUM_ADDRESS_OR_ENS_NAME_HPP

#include <string_view>
#include <variant>
#include "streamr-utils/ENSName.hpp"
#include "streamr-utils/EthereumAddress.hpp"

namespace streamr::utils {

inline std::variant<EthereumAddress, ENSName> toEthereumAddressOrENSName(
    std::string_view str) {
    if (isENSNameFormatIgnoreCase(str)) {
        return toENSName(str);
    }
    return toEthereumAddress(str);
}

} // namespace streamr::utils

#endif // STREAMR_UTILS_TO_ETHEREUM_ADDRESS_OR_ENS_NAME_HPP
