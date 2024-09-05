#ifndef STREAMR_UTILS_IPV4_HELPER_HPP
#define STREAMR_UTILS_IPV4_HELPER_HPP

#include <arpa/inet.h>
#include <array>
#include <string>

namespace streamr::utils {

class Ipv4Helper {
public:
    static uint32_t ipv4ToNumber(const std::string& ip) {
        uint32_t result;
        inet_pton(AF_INET, ip.c_str(), &result);
        return result;
    }
    static std::string numberToIpv4(uint32_t value) {
        std::array<char, INET_ADDRSTRLEN> buffer;
        inet_ntop(AF_INET, &value, buffer.data(), INET_ADDRSTRLEN);
        return std::string{buffer.data()};
    }
};

} // namespace streamr::utils

#endif // STREAMR_UTILS_IPV4_HELPER_HPP