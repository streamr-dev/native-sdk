#ifndef STREAMR_DHT_VERSION_HPP
#define STREAMR_DHT_VERSION_HPP

#include <optional>
#include <ranges>
#include <string>
#include <vector>

namespace streamr::dht::helpers {

struct VersionNumber {
    size_t major;
    size_t minor;
};

class Version {
public:
    static constexpr auto localProtocolVersion = "1.1";

    static bool isMaybeSupportedVersion(const std::string& remoteVersion) {
        const auto localVersionNumber = parseVersion(localProtocolVersion);
        const auto remoteVersionNumber = parseVersion(remoteVersion);

        return (localVersionNumber.has_value() && remoteVersionNumber.has_value() &&
            (remoteVersionNumber.value().major >= localVersionNumber.value().major)); 
    }

    static std::optional<VersionNumber> parseVersion(
        const std::string& version) {
        const std::vector<std::string> parts = version |
            std::views::split('.') |
            std::ranges::to<std::vector<std::string>>();
        if (parts.size() == 2) {
            const std::vector<size_t> values = parts |
                std::views::transform([](const std::string& p) {
                                                   return std::stoul(p);
                                               }) |
                std::ranges::to<std::vector<size_t>>();

            return VersionNumber{.major = values[0], .minor = values[1]};
        }
        return std::nullopt;
    }
};

} // namespace streamr::dht::helpers

#endif // STREAMR_DHT_VERSION_HPP