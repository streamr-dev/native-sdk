#ifndef STREAMR_DHT_DHT_CONTACT_RPCREMOTE_HPP
#define STREAMR_DHT_DHT_CONTACT_RPCREMOTE_HPP

#include <chrono>
#include <optional>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/helpers/Connectivity.hpp"

namespace streamr::dht::contact {

using ::dht::PeerDescriptor;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::connection::ConnectionType;
using streamr::dht::helpers::Connectivity;

template <typename ProtoRpcClient>
class RpcRemote {
private:
    PeerDescriptor localPeerDescriptor;
    PeerDescriptor remotePeerDescriptor;
    ProtoRpcClient client;
    std::optional<std::chrono::milliseconds> timeout;

    
    // Should connect directly to the server, timeout can be low
    static constexpr std::chrono::milliseconds websocketClientTimeout{5000};
    // Requires a WebsocketConnectionRequest to be routed to the client before the
    // connection can be opened
    // takes a little bit longer than WEBSOCKET_CLIENT
    static constexpr std::chrono::milliseconds websocketServerTimeout{7500};
    // WebRTC connections require lots of signalling to open and might take a longer
    // time.
    static constexpr std::chrono::milliseconds webrtcTimeout{10000};
    // default timeout for existing connections
    static constexpr std::chrono::milliseconds existingConnectionTimeout{5000};

    static std::chrono::milliseconds calculateTimeout(const PeerDescriptor& localPeerDescriptor, const PeerDescriptor& remotePeerDescriptor) { // NOLINT
        const ConnectionType connectionType = Connectivity::expectedConnectionType(localPeerDescriptor, remotePeerDescriptor);
        if (connectionType == ConnectionType::WEBSOCKET_CLIENT) {
            return websocketClientTimeout;
        } 
        if (connectionType == ConnectionType::WEBSOCKET_SERVER) {
            return websocketServerTimeout;
        } 
        if (connectionType == ConnectionType::WEBRTC) {
            return webrtcTimeout;
        }
        return webrtcTimeout;
    }

protected:
    RpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        ProtoRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : localPeerDescriptor(std::move(localPeerDescriptor)),
          remotePeerDescriptor(std::move(remotePeerDescriptor)),
          timeout(timeout),
          client(std::move(client)) {
    }

public:
    [[nodiscard]] const PeerDescriptor& getPeerDescriptor() const {
        return this->remotePeerDescriptor;
    }

    [[nodiscard]] const PeerDescriptor& getLocalPeerDescriptor() const {
        return this->localPeerDescriptor;
    }

    ProtoRpcClient& getClient() {
        return this->client;
    }

    [[nodiscard]] DhtCallContext formDhtRpcOptions(
        std::optional<DhtCallContext> opts = std::nullopt) const {
        DhtCallContext ret;
        if (opts.has_value()) {
            ret = opts.value();
        }
        ret.sourceDescriptor = this->localPeerDescriptor;
        ret.targetDescriptor = this->remotePeerDescriptor;
        
        return ret;
    }

    [[nodiscard]] std::chrono::milliseconds getTimeout() const {
        if (this->timeout.has_value()) {
            return this->timeout.value();
        }
        return calculateTimeout(this->localPeerDescriptor, this->remotePeerDescriptor);
    }
};

} // namespace streamr::dht::contact

#endif