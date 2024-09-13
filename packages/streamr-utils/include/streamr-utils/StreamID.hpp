#ifndef STREAMR_UTILS_STREAM_ID_HPP
#define STREAMR_UTILS_STREAM_ID_HPP

#include <string>
#include <optional>
#include <variant>
#include "streamr-utils/Branded.hpp"
#include "streamr-utils/EthereumAddress.hpp"
#include "streamr-utils/ENSName.hpp"
#include "streamr-utils/toEthereumAddressOrENSName.hpp"

namespace streamr::utils {

using StreamID = Branded<std::string, struct StreamIDBrand>;

/**
 * Create an instance of `StreamID` from a given string stream id or path.
 *
 * Supported formats:
 *  - full stream id format, e.g., '0x0000000000000000000000000000000000000000/foo/bar' or 'name.eth/foo/bar'
 *  - path-only format, e.g. , '/foo/bar'
 *  - legacy format, e.g., '7wa7APtlTq6EC5iTCBy6dw'
 *
 *  If `streamIdOrPath` is not in path-only format, `domain` can be left undefined.
 */
inline StreamID toStreamID(std::string_view streamIdOrPath, const std::optional<std::variant<EthereumAddress, ENSName>>& domain = std::nullopt) {
    if (streamIdOrPath.empty()) {
        throw std::runtime_error("stream id may not be empty");
    }
    const auto firstSlashIdx = streamIdOrPath.find('/');
    if (firstSlashIdx == std::string_view::npos) { // legacy format
        return StreamID{std::string(streamIdOrPath)};
    } 
    if (firstSlashIdx == 0) { // path-only format
        if (!domain.has_value()) {
            throw std::runtime_error("path-only format \"" + std::string(streamIdOrPath) + "\" provided without domain");
        }
        return std::visit([&streamIdOrPath](const auto& value) {
            return StreamID{std::string(value) + std::string(streamIdOrPath)};
        }, domain.value());
    }
    const auto dom = toEthereumAddressOrENSName(streamIdOrPath.substr(0, firstSlashIdx));
    const auto path = streamIdOrPath.substr(firstSlashIdx);

    return std::visit([&path](const auto& value) {
        return StreamID{std::string(value) + std::string(path)};
    }, dom);
}

class StreamIDUtils {

    static bool isPathOnlyFormat(std::string_view streamIdOrPath) {
        return streamIdOrPath.starts_with('/');
    }

    static std::optional<std::string> getDomain(const StreamID& streamId) {
        const auto domainAndPath = StreamIDUtils::getDomainAndPath(streamId);
        if (domainAndPath.has_value()) {
            return domainAndPath->first;
        }
        return std::nullopt;
    }
    
    static std::optional<std::string> getPath(const StreamID& streamId) {
        const auto domainAndPath = StreamIDUtils::getDomainAndPath(streamId);
        if (domainAndPath.has_value()) {
            return domainAndPath->second;
        }
        return std::nullopt;
    }
    
    static std::optional<std::pair<std::string, std::string>> getDomainAndPath(const StreamID& streamId) {
        const auto firstSlashIdx = streamId.find('/');
        if (firstSlashIdx != std::string::npos) {
            const auto domain = streamId.substr(0, firstSlashIdx);
            return std::make_pair(domain, streamId.substr(firstSlashIdx));
        } 
        return std::nullopt;
    }
};

} // namespace streamr::utils

#endif // STREAMR_UTILS_STREAM_ID_HPP