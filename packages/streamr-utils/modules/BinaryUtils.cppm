// Module streamr.utils.BinaryUtils
// CONSOLIDATED from the former header
// streamr-utils/BinaryUtils.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <boost/algorithm/hex.hpp>

export module streamr.utils.BinaryUtils;

import std;

export namespace streamr::utils {

class BinaryUtils {
public:
    static std::string binaryStringToHex(
        const std::string& binaryString, bool addPrefix = false) {
        std::string result;
        boost::algorithm::hex_lower(
            binaryString.begin(),
            binaryString.end(),
            std::back_inserter(result));
        if (addPrefix) {
            return "0x" + result;
        }
        return result;
    }

    static std::string hexToBinaryString(const std::string& hex) {
        std::string hexString = hex;
        if (hexString.starts_with("0x")) {
            hexString = hexString.substr(2);
        }

        if (hexString.length() % 2 != 0) {
            throw std::runtime_error(
                "Hex string length must be even, received: 0x" + hexString);
        }
        std::string result;
        boost::algorithm::unhex(hexString, std::back_inserter(result));

        if (hexString.length() != result.length() * 2) {
            throw std::runtime_error(
                "Hex string input is likely malformed, received: 0x" +
                hexString);
        }
        return result;
    }
};

} // namespace streamr::utils