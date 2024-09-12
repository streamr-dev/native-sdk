#ifndef STREAMR_UTILS_STREAM_PART_ID_HPP
#define STREAMR_UTILS_STREAM_PART_ID_HPP

#include <string>
#include <cstdint>
#include "streamr-utils/Branded.hpp"

namespace streamr::utils {

constexpr std::string_view DELIMITER = "#"; //NOLINT

using StreamPartID = Branded<std::string, struct StreamPartIDBrand>;


class StreamPartIDUtils {
    static toStreamPartID(streamId: StreamID, streamPartition: number): StreamPartID | never {
    ensureValidStreamPartitionIndex(streamPartition)
    return `${streamId}${DELIMITER}${streamPartition}` as StreamPartID
}
    static parse(streamPartIdAsStr: string): StreamPartID | never {
        const [streamId, streamPartition] = StreamPartIDUtils.parseRawElements(streamPartIdAsStr)
        if (streamPartition === undefined) {
            throw new Error(`invalid streamPartId string: ${streamPartIdAsStr}`)
        }
        toStreamID(streamId) // throws if not valid
        ensureValidStreamPartitionIndex(streamPartition)
        return streamPartIdAsStr as StreamPartID
    }

    static getStreamID(streamPartId: StreamPartID): StreamID {
        return this.getStreamIDAndPartition(streamPartId)[0]
    }

    static getStreamPartition(streamPartId: StreamPartID): number {
        return this.getStreamIDAndPartition(streamPartId)[1]
    }

    static getStreamIDAndPartition(streamPartId: StreamPartID): [StreamID, number] {
        return StreamPartIDUtils.parseRawElements(streamPartId) as [StreamID, number]
    }

    static parseRawElements(str: string): [string, number | undefined] {
        const lastIdx = str.lastIndexOf(DELIMITER)
        if (lastIdx === -1 || lastIdx === str.length - 1) {
            return [str, undefined]
        }
        return [str.substring(0, lastIdx), Number(str.substring(lastIdx + 1))]
    }
}


} // namespace streamr::utils

#endif // STREAMR_UTILS_STREAM_PART_ID_HPP