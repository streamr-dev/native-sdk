#ifndef STREAMR_UTILS_TO_ETHEREUM_ADDRESS_OR_ENS_NAME_HPP
#define STREAMR_UTILS_TO_ETHEREUM_ADDRESS_OR_ENS_NAME_HPP

#include "streamr-utils/EthereumAddress.hpp"
#include "streamr-utils/ENSName.hpp"
#include <variant>
#include <string_view>

namespace streamr::utils {

inline std::variant<EthereumAddress, ENSName> toEthereumAddressOrENSName(std::string_view str) {
    if (isENSNameFormatIgnoreCase(str)) {
        return toENSName(str);
    }
    return toEthereumAddress(str);
}

}

#endif // STREAMR_UTILS_TO_ETHEREUM_ADDRESS_OR_ENS_NAME_HPP
