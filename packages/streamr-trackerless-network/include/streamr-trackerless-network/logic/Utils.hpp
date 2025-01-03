#ifndef STREAMR_TRACKERLESS_NETWORK_UTILS_HPP
#define STREAMR_TRACKERLESS_NETWORK_UTILS_HPP

#include <map>
#include <optional>
#include <string>
#include <boost/algorithm/hex.hpp>
#include "DuplicateMessageDetector.hpp"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"

namespace streamr::trackerlessnetwork {

using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;

class Utils {
public:
    static bool markAndCheckDuplicate(
        std::map<std::string, DuplicateMessageDetector>& duplicateDetectors,
        const MessageID& currentMessage,
        const std::optional<MessageRef>& previousMessageRef) {
        const auto& publisherIdRaw = currentMessage.publisherid();
        auto publisherId =
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{publisherIdRaw});
        const auto& detectorKey =
            publisherId + "-" + currentMessage.messagechainid();

        std::optional<NumberPair> previousNumberPair =
            previousMessageRef.has_value()
            ? std::make_optional(NumberPair(
                  previousMessageRef.value().timestamp(),
                  previousMessageRef.value().sequencenumber()))
            : std::nullopt;

        NumberPair currentNumberPair(
            currentMessage.timestamp(), currentMessage.sequencenumber());
        if (!duplicateDetectors.contains(detectorKey)) {
            duplicateDetectors.emplace(detectorKey, DuplicateMessageDetector{});
        }
        return duplicateDetectors.at(detectorKey)
            .markAndCheck(previousNumberPair, currentNumberPair);
    }
};

} // namespace streamr::trackerlessnetwork

#endif // STREAMR_TRACKERLESS_NETWORK_UTILS_HPP