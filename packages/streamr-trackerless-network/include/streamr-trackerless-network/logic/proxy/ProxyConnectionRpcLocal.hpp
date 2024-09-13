#ifndef STREAMR_TRACKERLESS_NETWORK_PROXY_PROXYCONNECTIONRPCCLIENT_HPP
#define STREAMR_TRACKERLESS_NETWORK_PROXY_PROXYCONNECTIONRPCCLIENT_HPP

#include <folly/coro/blockingWait.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "packages/network/protos/NetworkRpc.server.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-trackerless-network/logic/ContentDeliveryRpcRemote.hpp"
#include "streamr-utils/BinaryUtils.hpp"
#include "streamr-utils/EthereumAddress.hpp"
#include "streamr-utils/StreamPartID.hpp"

namespace streamr::trackerlessnetwork::proxy {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::trackerlessnetwork::ContentDeliveryRpcRemote;
using streamr::utils::BinaryUtils;
using streamr::utils::EthereumAddress;
using streamr::utils::StreamPartID;
using streamr::utils::toEthereumAddress;
using ProxyConnectionRpc =
    streamr::protorpc::ProxyConnectionRpc<DhtCallContext>;

struct ProxyConnectionRpcLocalOptions {
    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    ListeningRpcCommunicator& rpcCommunicator;
};

struct NewConnection : Event<DhtAddress> {};

using ProxyConnectionRpcLocalEvents = std::tuple<NewConnection>;

class ProxyConnectionRpcLocal
    : public EventEmitter<ProxyConnectionRpcLocalEvents>,
      public ProxyConnectionRpc {
private:
    struct ProxyConnection {
        ProxyDirection
            direction; // Direction is from the client's point of view
        EthereumAddress userId;
        ContentDeliveryRpcRemote remote;
    };

    ProxyConnectionRpcLocalOptions options;
    std::map<DhtAddress, std::shared_ptr<ProxyConnection>> connections;

    std::vector<DhtAddress> getNodeIdsForUserId(
        const EthereumAddress& userId) const {
        return connections | std::views::keys |
            std::views::filter([&](const auto& nodeId) {
                   return connections.at(nodeId)->userId == userId;
               }) |
            std::ranges::to<std::vector>();
    }

    std::vector<DhtAddress> getSubscribers() const {
        return connections | std::views::keys |
            std::views::filter([&](const auto& nodeId) {
                   return connections.at(nodeId)->direction ==
                       ProxyDirection::SUBSCRIBE;
               }) |
            std::ranges::to<std::vector>();
    }

public:
    explicit ProxyConnectionRpcLocal(ProxyConnectionRpcLocalOptions options)
        : options(std::move(options)) {
        this->options.rpcCommunicator
            .registerRpcMethod<ProxyConnectionRequest, ProxyConnectionResponse>(
                "requestConnection",
                [this](
                    const ProxyConnectionRequest& msg,
                    const DhtCallContext& context) {
                    return this->requestConnection(msg, context);
                });
    }

    std::optional<std::shared_ptr<ProxyConnection>> getConnection(
        const DhtAddress& nodeId) const {
        auto it = this->connections.find(nodeId);
        if (it != this->connections.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    bool hasConnection(const DhtAddress& nodeId) const {
        return this->connections.find(nodeId) != this->connections.end();
    }

    void removeConnection(const DhtAddress& nodeId) {
        this->connections.erase(nodeId);
    }

    void stop() {
        for (const auto& connection : this->connections) {
            folly::coro::blockingWait(
                connection.second->remote.leaveStreamPartNotice(
                    this->options.streamPartId, false));
        }
        this->connections.clear();
        this->removeAllListeners();
    }

    std::vector<DhtAddress> getPropagationTargets(
        const StreamMessage& msg) const {
        if (msg.has_groupkeyrequest()) {
            try {
                const auto recipientId = msg.groupkeyrequest().recipientid();
                return this->getNodeIdsForUserId(toEthereumAddress(
                    BinaryUtils::binaryStringToHex(recipientId, true)));
            } catch (const std::exception& err) {
                SLogger::trace(
                    "Could not parse GroupKeyRequest " +
                    std::string(err.what()));
                return {};
            }
        } else {
            return this->getSubscribers();
        }
    }

    // IProxyConnectionRpc server method
    ProxyConnectionResponse requestConnection(
        const ProxyConnectionRequest& request,
        const DhtCallContext& context) override {
        const auto senderPeerDescriptor =
            context.incomingSourceDescriptor.value();
        const auto remoteNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(senderPeerDescriptor);

        this->connections[remoteNodeId] =
            std::make_shared<ProxyConnection>(ProxyConnection{
                .direction = request.direction(),
                .userId = toEthereumAddress(
                    BinaryUtils::binaryStringToHex(request.userid(), true)),
                .remote = std::move(ContentDeliveryRpcRemote(
                    this->options.localPeerDescriptor,
                    senderPeerDescriptor,
                    ContentDeliveryRpcClient{this->options.rpcCommunicator}))});
        ProxyConnectionResponse response;
        response.set_accepted(true);

        SLogger::trace(
            "Accepted connection request from " + remoteNodeId + " to " +
            this->options.streamPartId);
        this->emit<NewConnection>(remoteNodeId);
        return response;
    }
};
} // namespace streamr::trackerlessnetwork::proxy

#endif // STREAMR_TRACKERLESS_NETWORK_PROXY_PROXYCONNECTIONRPCCLIENT_HPP