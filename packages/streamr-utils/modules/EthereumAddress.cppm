// Module streamr.utils.EthereumAddress
// CONSOLIDATED from the former header
// streamr-utils/EthereumAddress.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;


export module streamr.utils.EthereumAddress;

import std;

import streamr.utils.Branded;

export namespace streamr::utils {

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
