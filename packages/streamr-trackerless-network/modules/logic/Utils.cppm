// Module streamr.trackerlessnetwork.Utils
// CONSOLIDATED from the former header logic/Utils.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <boost/algorithm/hex.hpp>
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.Utils;

import std;

import streamr.dht.Identifiers;
import streamr.trackerlessnetwork.DuplicateMessageDetector;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
export namespace streamr::trackerlessnetwork {

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
