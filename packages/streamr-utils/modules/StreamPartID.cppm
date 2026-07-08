// Module streamr.utils.StreamPartID
// CONSOLIDATED from the former header
// streamr-utils/StreamPartID.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;


export module streamr.utils.StreamPartID;

import std;

import streamr.utils.Branded;
import streamr.utils.StreamID;
import streamr.utils.partition;

export namespace streamr::utils {

inline constexpr auto DELIMITER = "#"; // NOLINT

using StreamPartID = Branded<std::string, struct StreamPartIDBrand>;

inline StreamPartID toStreamPartID(
    const StreamID& streamId, std::uint32_t streamPartition) {
    ensureValidStreamPartitionIndex(streamPartition);
    return StreamPartID(streamId + DELIMITER + std::to_string(streamPartition));
}

class StreamPartIDUtils {
public:
    static StreamPartID parse(const std::string& streamPartIdAsStr) {
        const auto [streamId, streamPartition] =
            StreamPartIDUtils::parseRawElements(streamPartIdAsStr);
        if (streamPartition == std::nullopt) {
            throw std::invalid_argument(
                "invalid streamPartId string: " + streamPartIdAsStr);
        }
        toStreamID(streamId); // throws if not valid
        ensureValidStreamPartitionIndex(streamPartition);
        return StreamPartID{streamPartIdAsStr};
    }

    static StreamID getStreamID(const StreamPartID& streamPartId) {
        return StreamPartIDUtils::getStreamIDAndPartition(streamPartId).first;
    }

    static std::optional<std::uint32_t> getStreamPartition(
        const StreamPartID& streamPartId) {
        return StreamPartIDUtils::getStreamIDAndPartition(streamPartId).second;
    }

    static std::pair<StreamID, std::optional<std::uint32_t>> getStreamIDAndPartition(
        const StreamPartID& streamPartId) {
        auto elements = StreamPartIDUtils::parseRawElements(streamPartId);
        return {StreamID(elements.first), elements.second};
    }

    static std::pair<std::string, std::optional<std::uint32_t>> parseRawElements(
        const std::string& str) {
        const auto lastIdx = str.find_last_of(DELIMITER);
        if (lastIdx == std::string::npos || lastIdx == str.length() - 1) {
            return {str, std::nullopt};
        }
        return {str.substr(0, lastIdx), std::stoul(str.substr(lastIdx + 1))};
    }
};

} // namespace streamr::utils