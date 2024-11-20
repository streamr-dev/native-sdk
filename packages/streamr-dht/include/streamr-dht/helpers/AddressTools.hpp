#ifndef ADDRESS_TOOLS_HPP
#define ADDRESS_TOOLS_HPP

#include <optional>
#include <ranges>
#include <string>
#include <vector>
#include <ipaddress/errors.hpp>
#include <ipaddress/ipaddress.hpp>

namespace streamr::dht::helpers {

class AddressTools {
public:
    static bool isPrivateIPv4(const std::string& address) {
        ipaddress::error_code ec;
        auto ipaddr = ipaddress::ipv4_address::parse(address, ec);

        return (ec == ipaddress::error_code::no_error && ipaddr.is_private());
    }

    static std::optional<std::string> getAddressFromIceCandidate(
        const std::string& candidate) {
        auto fields = candidate | std::views::split(' ') |
            std::views::filter([](auto field) { return !field.empty(); }) |
            std::ranges::to<std::vector<std::string>>();

        if (fields.size() >= 5) { // NOLINT
            auto ipField = fields[4];
            ipaddress::error_code ec;
            auto ipaddr = ipaddress::ipv4_address::parse(ipField, ec);
            if (ec == ipaddress::error_code::no_error) {
                return ipField;
            }
        }
        return std::nullopt;
    }
};

} // namespace streamr::dht::helpers

#endif // ADDRESS_TOOLS_HPP