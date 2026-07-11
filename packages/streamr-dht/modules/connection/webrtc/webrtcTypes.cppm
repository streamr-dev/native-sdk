// Module streamr.dht.webrtcTypes
// Ported from packages/dht/src/connection/webrtc/types.ts, consts.ts,
// iceServerAsString.ts and the event/ctor-parameter shapes of
// IWebrtcConnection.ts / types/WebrtcConnectionParams.ts (v103.8.0-rc.3).
// iceServerAsString produces the URL form libdatachannel's rtc::IceServer
// constructor parses (the TS node-datachannel binding feeds the same
// strings to the same parser) — wire/config-visible, keep exact.
module;

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

export module streamr.dht.webrtcTypes;

import streamr.dht.protos;

import streamr.dht.PortRange;
import streamr.eventemitter.EventEmitter;

export namespace streamr::dht::connection::webrtc {

using ::dht::PeerDescriptor;

struct IceServer {
    std::string url;
    uint16_t port = 0;
    std::optional<std::string> username = std::nullopt;
    std::optional<std::string> password = std::nullopt;
    std::optional<bool> tcp = std::nullopt;
};

// TS consts.ts EARLY_TIMEOUT: how long a started connection may wait for
// the remote description before it is torn down.
inline constexpr std::chrono::milliseconds earlyTimeout{5000};

// TS RtcDescription enum.
namespace rtcdescription {
inline constexpr auto OFFER = "offer";
inline constexpr auto ANSWER = "answer";
} // namespace rtcdescription

inline std::string iceServerAsString(const IceServer& iceServer) {
    const auto separator = iceServer.url.find(':');
    if (separator == std::string::npos ||
        separator + 1 >= iceServer.url.size()) {
        throw std::runtime_error("invalid stun/turn format: " + iceServer.url);
    }
    const std::string protocol = iceServer.url.substr(0, separator);
    const std::string hostname = iceServer.url.substr(separator + 1);
    if (!iceServer.username.has_value() && !iceServer.password.has_value()) {
        return protocol + ":" + hostname + ":" + std::to_string(iceServer.port);
    }
    if (iceServer.username.has_value() && iceServer.password.has_value()) {
        return protocol + ":" + *iceServer.username + ":" +
            *iceServer.password + "@" + hostname + ":" +
            std::to_string(iceServer.port) +
            (iceServer.tcp.has_value() ? "?transport=tcp" : "");
    }
    throw std::runtime_error(
        "username (" + iceServer.username.value_or("undefined") +
        ") and password (" + iceServer.password.value_or("undefined") +
        ") must be supplied together");
}

// TS WebrtcConnectionEvents extends ConnectionEvents with these two; the
// C++ Connection base fixes its event tuple, so WebrtcConnection exposes
// them through a separate member emitter (webrtcEvents()).
namespace webrtcconnectionevents {
struct LocalDescription
    : streamr::eventemitter::
          Event<std::string /*description*/, std::string /*type*/> {};
struct LocalCandidate
    : streamr::eventemitter::
          Event<std::string /*candidate*/, std::string /*mid*/> {};
} // namespace webrtcconnectionevents

using WebrtcSignallingEvents = std::tuple<
    webrtcconnectionevents::LocalDescription,
    webrtcconnectionevents::LocalCandidate>;

// TS types/WebrtcConnectionParams.ts.
struct WebrtcConnectionParams {
    PeerDescriptor remotePeerDescriptor;
    std::optional<size_t> bufferThresholdHigh = std::nullopt;
    std::optional<size_t> bufferThresholdLow = std::nullopt;
    std::optional<size_t> maxMessageSize = std::nullopt;
    std::vector<IceServer> iceServers = {};
    std::optional<streamr::dht::types::PortRange> portRange = std::nullopt;
};

} // namespace streamr::dht::connection::webrtc
