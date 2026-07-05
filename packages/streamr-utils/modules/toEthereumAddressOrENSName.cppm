// Module streamr.utils.toEthereumAddressOrENSName
// CONSOLIDATED from the former header
// streamr-utils/toEthereumAddressOrENSName.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <string>
#include <string_view>
#include <variant>

export module streamr.utils.toEthereumAddressOrENSName;

import streamr.utils.ENSName;
import streamr.utils.EthereumAddress;

export namespace streamr::utils {

inline std::variant<EthereumAddress, ENSName> toEthereumAddressOrENSName(
    std::string_view str) {
    if (isENSNameFormatIgnoreCase(str)) {
        return toENSName(str);
    }
    return toEthereumAddress(str);
}

} // namespace streamr::utils