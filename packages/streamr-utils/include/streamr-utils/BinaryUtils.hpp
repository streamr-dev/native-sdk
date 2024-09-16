#ifndef STREAMR_UTILS_BINARYUTILS_HPP
#define STREAMR_UTILS_BINARYUTILS_HPP

#include <sstream>
#include <string>
#include <boost/algorithm/hex.hpp>

namespace streamr::utils {

class BinaryUtils {
public:
    static std::string binaryStringToHex(
        const std::string& binaryString, bool addPrefix = false) {
        std::stringstream res;
        for (const auto& byte : binaryString) {
            res << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(byte);
        }
        if (addPrefix) {
            return "0x" + res.str();
        }
        return res.str();
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

#endif // STREAMR_UTILS_BINARYUTILS_HPP