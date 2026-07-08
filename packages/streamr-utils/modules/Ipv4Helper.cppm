// Module streamr.utils.Ipv4Helper
// CONSOLIDATED from the former header
// streamr-utils/Ipv4Helper.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;
#include <new>

#include <arpa/inet.h>

export module streamr.utils.Ipv4Helper;

import std;

export namespace streamr::utils {

class Ipv4Helper {
public:
    static std::uint32_t ipv4ToNumber(const std::string& ip) {
        std::uint32_t result;
        inet_pton(AF_INET, ip.c_str(), &result);
        return result;
    }
    static std::string numberToIpv4(std::uint32_t value) {
        std::array<char, INET_ADDRSTRLEN> buffer;
        inet_ntop(AF_INET, &value, buffer.data(), INET_ADDRSTRLEN);
        return std::string{buffer.data()};
    }
};

} // namespace streamr::utils
