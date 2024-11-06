#ifndef STREAMR_DHT_HELPERS_CONNECTIVITY_HPP
#define STREAMR_DHT_HELPERS_CONNECTIVITY_HPP

#include <optional>
#include <string>
#include <streamr-dht/connection/Connection.hpp>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/helpers/AddressTools.hpp"

namespace streamr::dht::helpers {

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

#endif // STREAMR_DHT_HELPERS_CONNECTIVITY_HPP