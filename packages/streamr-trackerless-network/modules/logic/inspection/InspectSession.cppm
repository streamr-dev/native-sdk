// Module streamr.trackerlessnetwork.InspectSession
// Ported from packages/trackerless-network/src/content-delivery-layer/
// inspection/InspectSession.ts (v103.8.0-rc.3): tracks which of the
// messages seen during an inspection were (also) delivered by the
// inspected node, and emits Done as soon as the inspected node proves it
// forwards traffic. (The C++ package keeps TS's content-delivery-layer
// classes under modules/logic/, matching the existing tree.)
module;

#include <algorithm>
#include <cstddef>
#include <map>
#include <mutex>
#include <ranges>
#include <string>
#include <tuple>
#include <utility>

export module streamr.trackerlessnetwork.InspectSession;

import streamr.trackerlessnetwork.protos;

import streamr.dht.Identifiers;
import streamr.eventemitter.EventEmitter;
import streamr.utils.BinaryUtils;

export namespace streamr::trackerlessnetwork::inspection {

using streamr::dht::DhtAddress;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::utils::BinaryUtils;

namespace inspectsessionevents {

struct Done : Event<> {};

} // namespace inspectsessionevents

using InspectSessionEvents = std::tuple<inspectsessionevents::Done>;

struct InspectSessionOptions {
    DhtAddress inspectedNode;
};

class InspectSession : public EventEmitter<InspectSessionEvents> {
private:
    // TS runs single-threaded; here markMessage() arrives from delivery
    // threads while inspect() reads the counts, so the map is guarded.
    // Done is emitted OUTSIDE the lock (listeners like waitForEvent take
    // their own locks).
    std::mutex mutex;
    // Value: has the message been received from the inspected node.
    std::map<std::string, bool> inspectionMessages;
    DhtAddress inspectedNode;

    static std::string createMessageKey(const ::MessageID& messageId) {
        // TS toUserId(publisherId) renders the bytes as lowercase hex.
        return BinaryUtils::binaryStringToHex(messageId.publisherid()) + ":" +
            messageId.messagechainid() + ":" +
            std::to_string(messageId.timestamp()) + ":" +
            std::to_string(messageId.sequencenumber());
    }

public:
    explicit InspectSession(InspectSessionOptions options)
        : inspectedNode(std::move(options.inspectedNode)) {}

    void markMessage(
        const DhtAddress& remoteNodeId, const ::MessageID& messageId) {
        bool done = false;
        {
            std::scoped_lock lock(this->mutex);
            const auto messageKey = createMessageKey(messageId);
            const auto it = this->inspectionMessages.find(messageKey);
            if (it == this->inspectionMessages.end()) {
                this->inspectionMessages.emplace(
                    messageKey, remoteNodeId == this->inspectedNode);
            } else if (!it->second && remoteNodeId == this->inspectedNode) {
                done = true;
            } else if (it->second) {
                done = true;
            }
        }
        if (done) {
            this->emit<inspectsessionevents::Done>();
        }
    }

    [[nodiscard]] size_t getInspectedMessageCount() {
        std::scoped_lock lock(this->mutex);
        return this->inspectionMessages.size();
    }

    [[nodiscard]] bool onlyMarkedByInspectedNode() {
        std::scoped_lock lock(this->mutex);
        return std::ranges::all_of(
            this->inspectionMessages,
            [](const auto& entry) { return entry.second; });
    }

    void stop() { this->emit<inspectsessionevents::Done>(); }
};

} // namespace streamr::trackerlessnetwork::inspection
