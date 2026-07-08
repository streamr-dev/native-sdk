// Module streamr.dht.Version
// CONSOLIDATED from the former header streamr-dht/helpers/Version.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;


export module streamr.dht.Version;

import std;
export namespace streamr::dht::helpers {

struct VersionNumber {
    std::size_t major;
    std::size_t minor;
};

class Version {
public:
    static constexpr auto localProtocolVersion = "1.1";
    // Sent in HandshakeRequest/Response applicationVersion (informational,
    // never used for compatibility checks); the TS implementation sends its
    // package version here.
    static constexpr auto localApplicationVersion = "1.0.0";

    static bool isMaybeSupportedVersion(const std::string& remoteVersion) {
        const auto localVersionNumber = parseVersion(localProtocolVersion);
        const auto remoteVersionNumber = parseVersion(remoteVersion);

        return (
            localVersionNumber.has_value() && remoteVersionNumber.has_value() &&
            (remoteVersionNumber.value().major >=
             localVersionNumber.value().major));
    }

    static std::optional<VersionNumber> parseVersion(
        const std::string& version) {
        const std::vector<std::string> parts = version |
            std::views::split('.') |
            std::ranges::to<std::vector<std::string>>();
        if (parts.size() == 2) {
            const std::vector<std::size_t> values = parts |
                std::views::transform([](const std::string& p) {
                                                   return std::stoul(p);
                                               }) |
                std::ranges::to<std::vector<std::size_t>>();

            return VersionNumber{.major = values[0], .minor = values[1]};
        }
        return std::nullopt;
    }
};

} // namespace streamr::dht::helpers
