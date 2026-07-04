// Module streamr.dht.Connectivity
// CONSOLIDATED from the former header streamr-dht/helpers/Connectivity.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <optional>
#include <string>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.Connectivity;

import streamr.dht.AddressTools;
import streamr.dht.Connection;
export namespace streamr::dht::helpers {

using ::dht::ConnectivityMethod;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectionType;
using streamr::dht::helpers::AddressTools;

class Connectivity {
public:
    static bool canOpenConnectionFromBrowser(
        const ConnectivityMethod& websocketServer) {
        const auto hasPrivateAddress =
            ((websocketServer.host() == "localhost") ||
             AddressTools::isPrivateIPv4(websocketServer.host()));
        return websocketServer.tls() || hasPrivateAddress;
    }

    static ConnectionType expectedConnectionType(
        const PeerDescriptor& localPeerDescriptor, // NOLINT
        const PeerDescriptor& remotePeerDescriptor) {
        if (remotePeerDescriptor.has_websocket() &&
            (localPeerDescriptor.type() != NodeType::BROWSER ||
             canOpenConnectionFromBrowser(remotePeerDescriptor.websocket()))) {
            return ConnectionType::WEBSOCKET_CLIENT;
        }
        if (localPeerDescriptor.has_websocket() &&
            (remotePeerDescriptor.type() != NodeType::BROWSER ||
             canOpenConnectionFromBrowser(localPeerDescriptor.websocket()))) {
            return ConnectionType::WEBSOCKET_SERVER;
        }
        return ConnectionType::WEBRTC;
    }

    static std::string connectivityMethodToWebsocketUrl(
        const ConnectivityMethod& ws,
        const std::optional<std::string>& action = std::nullopt) {
        return (ws.tls() ? "wss://" : "ws://") + ws.host() + ":" +
            std::to_string(ws.port()) +
            (action.has_value() ? "?action=" + action.value() : "");
    }
};

} // namespace streamr::dht::helpers
