#ifndef STREAMR_TRACKERLESS_NETWORK_PROXY_PROXY_CLIENT_HPP
#define STREAMR_TRACKERLESS_NETWORK_PROXY_PROXY_CLIENT_HPP

#include <map>
#include <optional>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/ConnectionLockStates.hpp"
#include "streamr-dht/connection/ConnectionLocker.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-trackerless-network/logic/ContentDeliveryRpcLocal.hpp"
#include "streamr-trackerless-network/logic/ContentDeliveryRpcRemote.hpp"
#include "streamr-trackerless-network/logic/DuplicateMessageDetector.hpp"
#include "streamr-trackerless-network/logic/NodeList.hpp"
#include "streamr-trackerless-network/logic/formStreamPartDeliveryServiceId.hpp"
#include "streamr-trackerless-network/logic/propagation/Propagation.hpp"
#include "streamr-trackerless-network/logic/proxy/ProxyConnectionRpcLocal.hpp"
#include "streamr-trackerless-network/logic/proxy/ProxyConnectionRpcRemote.hpp"
#include "streamr-trackerless-network/logic/utils.hpp"
#include "streamr-utils/AbortController.hpp"
#include "streamr-utils/EthereumAddress.hpp"
#include "streamr-utils/RetryUtils.hpp"
#include "streamr-utils/StreamPartID.hpp"

namespace streamr::trackerlessnetwork::proxy {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::LockID;
using streamr::dht::transport::Transport;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::trackerlessnetwork::formStreamPartContentDeliveryServiceId;
using streamr::trackerlessnetwork::Utils;
using streamr::trackerlessnetwork::propagation::Propagation;
using streamr::trackerlessnetwork::propagation::PropagationOptions;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcLocal;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcRemote;
using streamr::utils::AbortController;
using streamr::utils::EthereumAddress;
using streamr::utils::RetryUtils;
using streamr::utils::StreamPartID;

struct ProxyClientOptions {
    Transport& transport;
    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    ConnectionLocker& connectionLocker;
    std::optional<size_t> minPropagationTargets;
};

struct ProxyDefinition {
    std::map<DhtAddress, PeerDescriptor> nodes;
    size_t connectionCount;
    ProxyDirection direction;
    EthereumAddress userId;
};

struct Message : Event<StreamMessage> {};

using ProxyClientEvents = std::tuple<Message>;

class ProxyClient : public EventEmitter<ProxyClientEvents> {
private:
    struct ProxyConnection {
        PeerDescriptor peerDescriptor;
        ProxyDirection direction;
    };

    ListeningRpcCommunicator rpcCommunicator;
    ContentDeliveryRpcLocal contentDeliveryRpcLocal;
    ProxyClientOptions options;
    std::map<std::string, DuplicateMessageDetector> duplicateDetectors;
    std::optional<ProxyDefinition> definition;
    std::map<DhtAddress, ProxyConnection> connections;
    Propagation propagation;
    NodeList neighbors;
    AbortController abortController;
    std::optional<eventemitter::HandlerToken> transportDisconnectedHandlerToken;

    constexpr static auto SERVICE_ID = "system/proxy-client"; // NOLINT

public:
    explicit ProxyClient(ProxyClientOptions options)
        : rpcCommunicator(
              std::move(
                  formStreamPartContentDeliveryServiceId(options.streamPartId)),
              options.transport),
          neighbors(
              Identifiers::getNodeIdFromPeerDescriptor(
                  options.localPeerDescriptor),
              1000), // NOLINT
          contentDeliveryRpcLocal(ContentDeliveryRpcLocalOptions{
              .localPeerDescriptor = options.localPeerDescriptor,
              .streamPartId = options.streamPartId,
              .markAndCheckDuplicate =
                  [this](const MessageID& msg, const MessageRef& prev) {
                      return Utils::markAndCheckDuplicate(
                          this->duplicateDetectors, msg, prev);
                  },
              .broadcast =
                  [this](
                      const StreamMessage& message,
                      const DhtAddress& previousNode) {
                      this->broadcast(message, previousNode);
                  },
              .onLeaveNotice =
                  [this](const DhtAddress& remoteNodeId, bool /*isLeaving*/) {
                      const auto contact = this->neighbors.get(remoteNodeId);
                      if (contact.has_value()) {
                          this->onNodeDisconnected(
                              contact.value()->getPeerDescriptor());
                      }
                  },

              .markForInspection = [](const DhtAddress& nodeId,
                                      const MessageID& message) {},
              .rpcCommunicator = this->rpcCommunicator}),
          propagation(PropagationOptions{
              .sendToNeighbor =
                  [this](
                      const DhtAddress& neighborId, const StreamMessage& msg) {
                      const auto remote = this->neighbors.get(neighborId);
                      if (remote.has_value()) {
                          folly::coro::blockingWait(
                              remote.value()->sendStreamMessage(msg));
                      } else {
                          throw std::runtime_error(
                              "Propagation target not found");
                      }
                  },
              .minPropagationTargets = options.minPropagationTargets.has_value()
                  ? options.minPropagationTargets.value()
                  : 2,
          }),
          options(std::move(options)) {}

private:
    void registerDefaultServerMethods() {
        this->rpcCommunicator.registerRpcNotification<StreamMessage>(
            "sendStreamMessage",
            [this](const StreamMessage& msg, const DhtCallContext& context) {
                this->contentDeliveryRpcLocal.sendStreamMessage(msg, context);
            });
        this->rpcCommunicator.registerRpcNotification<LeaveStreamPartNotice>(
            "leaveStreamPartNotice",
            [this](
                const LeaveStreamPartNotice& req,
                const DhtCallContext& context) {
                this->contentDeliveryRpcLocal.leaveStreamPartNotice(
                    req, context);
            });
    }

public:
    void setProxies(
        const std::vector<PeerDescriptor>& nodes,
        ProxyDirection direction,
        const EthereumAddress& userId,
        std::optional<size_t> connectionCount = std::nullopt) {
        SLogger::trace(
            "Setting proxies",
            {{"streamPartId", this->options.streamPartId},
             {"direction", direction},
             {"userId", userId},
             {"connectionCount", connectionCount}});
        if (connectionCount.has_value() &&
            connectionCount.value() > nodes.size()) {
            throw std::runtime_error(
                "Cannot set connectionCount above the size of the configured array of nodes");
        }
        std::map<DhtAddress, PeerDescriptor> nodesIds;
        for (const auto& node : nodes) {
            nodesIds[Identifiers::getNodeIdFromPeerDescriptor(node)] = node;
        }
        this->definition = ProxyDefinition{
            .nodes = nodesIds,
            .connectionCount = connectionCount.has_value()
                ? connectionCount.value()
                : nodes.size(),
            .direction = direction,
            .userId = userId,
        };

        this->updateConnections();
    }

    void updateConnections() {
        auto invalidConnections = this->getInvalidConnections();
        for (const auto& id : invalidConnections) {
            this->closeConnection(id);
        }
        auto connectionCountDiff =
            this->definition->connectionCount - this->connections.size();
        if (connectionCountDiff > 0) {
            this->openRandomConnections(connectionCountDiff);
        } else if (connectionCountDiff < 0) {
            this->closeRandomConnections(-connectionCountDiff);
        }
    }

    std::vector<DhtAddress> getInvalidConnections() {
        return this->connections | std::views::keys |
            std::views::filter([this](const auto& id) {
                   return !this->definition->nodes.contains(id) ||
                       this->definition->direction !=
                       this->connections.at(id).direction;
               }) |
            std::ranges::to<std::vector>();
    }

    void openRandomConnections(size_t connectionCount) {
        const auto proxiesToAttempt =
            this->definition->nodes | std::views::keys |
            std::views::filter([this](const auto& id) {
                return !this->connections.contains(id);
            }) |
            std::views::take(connectionCount) | std::ranges::to<std::vector>();

        for (const auto& id : proxiesToAttempt) {
            this->attemptConnection(
                id, this->definition->direction, this->definition->userId);
        }
    }

    void attemptConnection(
        const DhtAddress& nodeId,
        const ProxyDirection& direction,
        const EthereumAddress& userId) {
        const auto peerDescriptor = this->definition->nodes.at(nodeId);
        ProxyConnectionRpcClient client{this->rpcCommunicator};
        ProxyConnectionRpcRemote rpcRemote(
            this->options.localPeerDescriptor, peerDescriptor, client);

        const auto accepted = folly::coro::blockingWait(
            rpcRemote.requestConnection(direction, userId));

        if (accepted) {
            this->options.connectionLocker.lockConnection(
                peerDescriptor, LockID{SERVICE_ID});

            this->connections.emplace(
                nodeId, ProxyConnection{peerDescriptor, direction});

            ContentDeliveryRpcClient client{this->rpcCommunicator};
            const auto remote = std::make_shared<ContentDeliveryRpcRemote>(
                this->options.localPeerDescriptor, peerDescriptor, client);

            this->neighbors.add(remote);
            this->propagation.onNeighborJoined(nodeId);
            SLogger::info(
                "Open proxy connection",
                {{"nodeId", nodeId},
                 {"streamPartId", this->options.streamPartId}});
        } else {
            SLogger::warn(
                "Unable to open proxy connection",
                {{"nodeId", nodeId},
                 {"streamPartId", this->options.streamPartId}});
        }
    }

    void closeRandomConnections(size_t connectionCount) {
        std::vector<std::pair<DhtAddress, ProxyConnection>> proxiesToDisconnect;
        std::sample(
            this->connections.begin(),
            this->connections.end(),
            std::back_inserter(proxiesToDisconnect),
            connectionCount,
            std::mt19937{std::random_device{}()});

        for (const auto& nodeId : proxiesToDisconnect) {
            this->closeConnection(nodeId.first);
        }
    }

    void closeConnection(DhtAddress nodeId) {
        if (this->connections.contains(nodeId)) {
            SLogger::info(
                "Close proxy connection",
                {{"nodeId", nodeId},
                 {"streamPartId", this->options.streamPartId}});
            const auto server = this->neighbors.get(nodeId);
            if (server.has_value()) {
                folly::coro::blockingWait(server.value()->leaveStreamPartNotice(
                    this->options.streamPartId, false));
            }
            this->removeConnection(this->connections.at(nodeId).peerDescriptor);
        }
    }

    void removeConnection(const PeerDescriptor& peerDescriptor) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        this->options.connectionLocker.unlockConnection(
            peerDescriptor, LockID{SERVICE_ID});
        this->connections.erase(nodeId);
        this->neighbors.remove(nodeId);
    }

    void broadcast(
        const StreamMessage& msg,
        const std::optional<DhtAddress>& previousNode = std::nullopt) {
        if (!previousNode.has_value()) {
            Utils::markAndCheckDuplicate(
                this->duplicateDetectors,
                msg.messageid(),
                msg.previousmessageref());
        }
        this->emit<Message>(msg);
        this->propagation.feedUnseenMessage(
            msg, this->neighbors.getIds(), previousNode);
    }

    [[nodiscard]] bool hasConnection(
        const DhtAddress& nodeId, ProxyDirection direction) const {
        return this->connections.contains(nodeId) &&
            this->connections.at(nodeId).direction == direction;
    }

    [[nodiscard]] ProxyDirection getDirection() const {
        return this->definition->direction;
    }

    void onNodeDisconnected(const PeerDescriptor& peerDescriptor) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        if (this->connections.contains(nodeId)) {
            this->options.connectionLocker.unlockConnection(
                peerDescriptor, LockID{SERVICE_ID});
            this->removeConnection(peerDescriptor);
            folly::coro::blockingWait(RetryUtils::constantRetry<void>(
                [this]() -> void { this->updateConnections(); },
                "updating proxy connections",
                this->abortController));
        }
    }

    void start() {
        this->registerDefaultServerMethods();

        this->transportDisconnectedHandlerToken =
            this->options.transport
                .on<dht::transport::transportevents::Disconnected>(
                    [this](
                        const PeerDescriptor& peerDescriptor,
                        bool /*gracefulLeave*/) {
                        this->onNodeDisconnected(peerDescriptor);
                    });
    }

    void stop() {
        auto allNeighbors = this->neighbors.getAll();
        for (const auto& remote : allNeighbors) {
            this->options.connectionLocker.unlockConnection(
                remote->getPeerDescriptor(), LockID{SERVICE_ID});
            folly::coro::blockingWait(remote->leaveStreamPartNotice(
                this->options.streamPartId, false));
        }

        this->neighbors.stop();
        this->rpcCommunicator.destroy();
        this->connections.clear();
        this->abortController.abort();
        if (this->transportDisconnectedHandlerToken.has_value()) {
            this->options.transport
                .off<dht::transport::transportevents::Disconnected>(
                    this->transportDisconnectedHandlerToken.value());
        }
    }
};

} // namespace streamr::trackerlessnetwork::proxy

#endif // STREAMR_TRACKERLESS_NETWORK_PROXY_PROXY_CLIENT_HPP
