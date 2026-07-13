// Module streamr.trackerlessnetwork.ProxyClient
// CONSOLIDATED from the former header logic/proxy/ProxyClient.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

// std::coroutine_traits must be visible in every translation unit
// that defines OR instantiates a coroutine; it cannot arrive through
// an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
// Textual: entities reached only through an imported module's global
// module fragment are not reliably reachable; this unit's code calls
// streamr::utils::blockingWait and std::mt19937 directly. (The former
// header received both transitively from the headers it included.)

export module streamr.trackerlessnetwork.ProxyClient;

import streamr.trackerlessnetwork.protos;

import streamr.utils.CoroutineHelper;
import streamr.dht.DhtCallContext;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.ConnectionLockStates;
import streamr.dht.ConnectionLocker;
import streamr.dht.Identifiers;
import streamr.dht.Transport;
import streamr.dht.protos;
import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;
import streamr.utils.AbortController;
import streamr.utils.EthereumAddress;
import streamr.utils.StreamPartID;
import streamr.trackerlessnetwork.ContentDeliveryRpcLocal;
import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.DuplicateMessageDetector;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.Propagation;
import streamr.trackerlessnetwork.ProxyConnectionRpcLocal;
import streamr.trackerlessnetwork.ProxyConnectionRpcRemote;
import streamr.trackerlessnetwork.Utils;
import streamr.trackerlessnetwork.formStreamPartDeliveryServiceId;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::LockID;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::AbortController;
using streamr::utils::EthereumAddress;
using streamr::utils::StreamPartID;

using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;

export namespace streamr::trackerlessnetwork::proxy {

using ::dht::PeerDescriptor;
using streamr::trackerlessnetwork::formStreamPartContentDeliveryServiceId;
using streamr::trackerlessnetwork::Utils;
using streamr::trackerlessnetwork::propagation::Propagation;
using streamr::trackerlessnetwork::propagation::PropagationOptions;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcLocal;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcRemote;

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
    // TS parity: absent = bidirectional (accepts and publishes both ways).
    std::optional<ProxyDirection> direction;
    EthereumAddress userId;
};

struct Message : Event<StreamMessage> {};

using ProxyClientEvents = std::tuple<Message>;

class ConnectingToProxyError {
private:
    std::exception_ptr originalException;
    PeerDescriptor peerDescriptor;

public:
    explicit ConnectingToProxyError(
        const std::exception_ptr& originalException, // NOLINT
        PeerDescriptor peerDescriptor)
        : originalException(originalException),
          peerDescriptor(std::move(peerDescriptor)) {}

    [[nodiscard]] PeerDescriptor getPeerDescriptor() const {
        return this->peerDescriptor;
    }

    [[nodiscard]] std::exception_ptr getOriginalException() const {
        return this->originalException;
    }
};

class ProxyClient : public EventEmitter<ProxyClientEvents> {
private:
    struct ProxyConnection {
        PeerDescriptor peerDescriptor;
        std::optional<ProxyDirection> direction;
    };

    struct ProxyConnectionError {
        PeerDescriptor peerDescriptor;
        ProxyDirection direction;
    };

    ListeningRpcCommunicator rpcCommunicator;
    ContentDeliveryRpcLocal contentDeliveryRpcLocal;
    ProxyClientOptions options;
    std::map<std::string, DuplicateMessageDetector> duplicateDetectors;
    std::optional<ProxyDefinition> definition;
    std::map<DhtAddress, ProxyConnection> connections;
    // TS is single-threaded; here setProxies (API thread) races
    // onNodeDisconnected (transport event threads, e.g. the rtc poll
    // thread) over definition/connections/duplicateDetectors — seen as
    // a use-after-free reading definition->nodes keys. The mutex guards
    // STATE ONLY and is NEVER held across a blocking RPC wait: a
    // transport event thread parked on it may be the very thread an
    // in-flight send needs (deadlock). Snapshot under the lock, act
    // outside, re-check under the lock when committing. Recursive:
    // unlockConnection can emit Disconnected inline on the same thread,
    // re-entering onNodeDisconnected.
    mutable std::recursive_mutex mutex;
    bool stopped = false; // guarded by mutex
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
          contentDeliveryRpcLocal(
              ContentDeliveryRpcLocalOptions{
                  .localPeerDescriptor = options.localPeerDescriptor,
                  .streamPartId = options.streamPartId,
                  .markAndCheckDuplicate =
                      [this](
                          const MessageID& msg,
                          const std::optional<MessageRef>& prev) {
                          std::scoped_lock lock(this->mutex);
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
                      [this](
                          const DhtAddress& remoteNodeId, bool /*isLeaving*/) {
                          const auto contact =
                              this->neighbors.get(remoteNodeId);
                          if (contact.has_value()) {
                              this->onNodeDisconnected(
                                  contact.value()->getPeerDescriptor());
                          }
                      },

                  .markForInspection = [](const DhtAddress& nodeId,
                                          const MessageID& message) {},
                  .rpcCommunicator = this->rpcCommunicator}),
          propagation(
              PropagationOptions{
                  .sendToNeighbor =
                      [this](
                          const DhtAddress& neighborId,
                          const StreamMessage& msg) -> folly::coro::Task<void> {
                      const auto remote = this->neighbors.get(neighborId);
                      if (remote.has_value()) {
                          co_await remote.value()->sendStreamMessage(msg);
                      } else {
                          throw std::runtime_error(
                              "Propagation target not found");
                      }
                  },
                  .minPropagationTargets =
                      options.minPropagationTargets.has_value()
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
    std::pair<
        std::vector<ConnectingToProxyError> /* connection errors */,
        std::vector<PeerDescriptor> /* successfully connected proxies */>
    setProxies(
        const std::vector<PeerDescriptor>& nodes,
        std::optional<ProxyDirection> direction,
        const EthereumAddress& userId,
        std::optional<size_t> connectionCount = std::nullopt) {
        SLogger::trace(
            "Setting proxies",
            {{"streamPartId", this->options.streamPartId},
             {"connectionCount", connectionCount}});
        if (connectionCount.has_value() &&
            connectionCount.value() > nodes.size()) {
            throw std::runtime_error(
                "Cannot set connectionCount above the size of the configured array of nodes");
        }
        {
            std::scoped_lock lock(this->mutex);
            std::map<DhtAddress, PeerDescriptor> nodesIds;
            for (const auto& node : nodes) {
                nodesIds[Identifiers::getNodeIdFromPeerDescriptor(node)] = node;
            }
            this->definition = ProxyDefinition{
                .nodes = std::move(nodesIds),
                .connectionCount = connectionCount.has_value()
                    ? connectionCount.value()
                    : nodes.size(),
                .direction = direction,
                .userId = userId,
            };
        }
        return this->updateConnections();
    }

    std::pair<
        std::vector<ConnectingToProxyError> /* connection errors */,
        std::vector<PeerDescriptor> /* successfully connected proxies */>
    updateConnections() {
        std::vector<DhtAddress> invalidConnections;
        {
            std::scoped_lock lock(this->mutex);
            invalidConnections = this->getInvalidConnections();
        }
        for (const auto& id : invalidConnections) {
            this->closeConnection(id);
        }
        std::vector<ConnectingToProxyError> errors;
        std::vector<PeerDescriptor> successfullyConnected;
        // Signed on purpose: connectionCount below the current
        // connection count must take the shrink branch (size_t
        // subtraction would wrap).
        std::int64_t connectionCountDiff = 0;
        {
            std::scoped_lock lock(this->mutex);
            connectionCountDiff =
                static_cast<std::int64_t>(this->definition->connectionCount) -
                static_cast<std::int64_t>(this->connections.size());
        }
        if (connectionCountDiff > 0) {
            auto [errs, success] = this->openRandomConnections(
                static_cast<size_t>(connectionCountDiff));
            errors.insert(errors.end(), errs.begin(), errs.end());
            successfullyConnected.insert(
                successfullyConnected.end(), success.begin(), success.end());
        } else if (connectionCountDiff < 0) {
            this->closeRandomConnections(
                static_cast<size_t>(-connectionCountDiff));
        }

        return {errors, successfullyConnected};
    }

    // Call under this->mutex.
    std::vector<DhtAddress> getInvalidConnections() {
        return this->connections | std::views::keys |
            std::views::filter([this](const auto& id) {
                   return !this->definition->nodes.contains(id) ||
                       this->definition->direction !=
                       this->connections.at(id).direction;
               }) |
            std::ranges::to<std::vector>();
    }

    std::pair<
        std::vector<ConnectingToProxyError> /* connection errors */,
        std::vector<PeerDescriptor> /* successfully connected proxies */>
    openRandomConnections(size_t connectionCount) {
        std::vector<DhtAddress> proxiesToAttempt;
        std::optional<ProxyDirection> direction;
        EthereumAddress userId{""};
        {
            std::scoped_lock lock(this->mutex);
            proxiesToAttempt = this->definition->nodes | std::views::keys |
                std::views::filter([this](const auto& id) {
                                   return !this->connections.contains(id);
                               }) |
                std::views::take(connectionCount) |
                std::ranges::to<std::vector>();
            direction = this->definition->direction;
            userId = this->definition->userId;
        }
        std::vector<ConnectingToProxyError> errors;
        std::vector<PeerDescriptor> successfullyConnected;
        for (const auto& id : proxiesToAttempt) {
            try {
                this->attemptConnection(id, direction, userId);
                std::scoped_lock lock(this->mutex);
                const auto it = this->connections.find(id);
                if (it != this->connections.end()) {
                    successfullyConnected.push_back(it->second.peerDescriptor);
                }
            } catch (const ConnectingToProxyError& e) {
                errors.push_back(std::move(ConnectingToProxyError(e)));
            }
        }
        return {errors, successfullyConnected};
    }

    void attemptConnection(
        const DhtAddress& nodeId,
        std::optional<ProxyDirection> direction,
        const EthereumAddress& userId) {
        PeerDescriptor peerDescriptor;
        {
            std::scoped_lock lock(this->mutex);
            const auto it = this->definition->nodes.find(nodeId);
            if (it == this->definition->nodes.end()) {
                // setProxies changed the definition while this attempt
                // was queued.
                return;
            }
            peerDescriptor = it->second;
        }
        ProxyConnectionRpcClient client{this->rpcCommunicator};
        ProxyConnectionRpcRemote rpcRemote(
            this->options.localPeerDescriptor, peerDescriptor, client);

        auto accepted = false;
        try {
            accepted = streamr::utils::blockingWait(
                rpcRemote.requestConnection(direction, userId));
        } catch (const std::exception& e) {
            SLogger::warn(
                "Unable to open proxy connection",
                {{"nodeId", nodeId},
                 {"streamPartId", this->options.streamPartId}});
            throw ConnectingToProxyError(
                std::current_exception(), peerDescriptor);
        }
        if (accepted) {
            this->options.connectionLocker.lockConnection(
                peerDescriptor, LockID{SERVICE_ID});

            {
                std::scoped_lock lock(this->mutex);
                if (this->stopped) {
                    this->options.connectionLocker.unlockConnection(
                        peerDescriptor, LockID{SERVICE_ID});
                    return;
                }
                this->connections.emplace(
                    nodeId,
                    ProxyConnection{
                        .peerDescriptor = peerDescriptor,
                        .direction = direction});
            }

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
                "Unable to open proxy connection, connection lock rejected",
                {{"nodeId", nodeId},
                 {"streamPartId", this->options.streamPartId}});
            throw ConnectingToProxyError(
                std::current_exception(), peerDescriptor);
        }
    }

    void closeRandomConnections(size_t connectionCount) {
        std::vector<std::pair<DhtAddress, ProxyConnection>> proxiesToDisconnect;
        {
            std::scoped_lock lock(this->mutex);
            std::ranges::sample(
                this->connections,
                std::back_inserter(proxiesToDisconnect),
                static_cast<std::ptrdiff_t>(connectionCount),
                std::mt19937{std::random_device{}()});
        }
        for (const auto& nodeId : proxiesToDisconnect) {
            this->closeConnection(nodeId.first);
        }
    }

    void closeConnection(DhtAddress nodeId) {
        std::optional<PeerDescriptor> peerDescriptor;
        {
            std::scoped_lock lock(this->mutex);
            const auto it = this->connections.find(nodeId);
            if (it == this->connections.end()) {
                return;
            }
            peerDescriptor = it->second.peerDescriptor;
            this->connections.erase(it);
        }
        SLogger::info(
            "Close proxy connection",
            {{"nodeId", nodeId}, {"streamPartId", this->options.streamPartId}});
        const auto server = this->neighbors.get(nodeId);
        if (server.has_value()) {
            streamr::utils::blockingWait(server.value()->leaveStreamPartNotice(
                this->options.streamPartId, false));
        }
        this->options.connectionLocker.unlockConnection(
            peerDescriptor.value(), LockID{SERVICE_ID});
        this->neighbors.remove(nodeId);
    }

    /**
     * @brief Broadcast a message to all connected nodes.
     *
     * @param msg The message to broadcast.
     * @param previousNode The node that sent the message.
     * @return A pair of vectors, the first containing the DhtAddresses of the
     * proxy nodes that received the message immediately, and the second
     * containing the DhtAddresses of the proxy nodes that failed to receive
     * the message.
     */

    std::pair<
        std::vector<PeerDescriptor> /* failed to send to */,
        std::vector<PeerDescriptor> /* successfully sent to */>
    broadcast(
        const StreamMessage& msg,
        const std::optional<DhtAddress>& previousNode = std::nullopt) {
        SLogger::debug("ProxyClient::broadcast()");
        if (!previousNode.has_value()) {
            SLogger::debug(
                "ProxyClient::broadcast() calling Utils::markAndCheckDuplicate() on message: " +
                msg.DebugString());
            {
                std::scoped_lock lock(this->mutex);
                Utils::markAndCheckDuplicate(
                    this->duplicateDetectors, msg.messageid(), std::nullopt);
            }
        }
        SLogger::debug("ProxyClient::broadcast() emitting Message event");
        this->emit<Message>(msg);
        SLogger::debug(
            "ProxyClient::broadcast() calling propagation.feedUnseenMessage()");
        // feedUnseenMessage speaks node ids (TS parity); this public API
        // reports full descriptors, so map the ids back through the
        // neighbor list.
        const auto [failedIds, successfulIds] = streamr::utils::blockingWait(
            this->propagation.feedUnseenMessageAndCollect(
                msg, this->neighbors.getIds(), previousNode));
        const auto toDescriptors = [this](const std::vector<DhtAddress>& ids) {
            std::vector<PeerDescriptor> descriptors;
            for (const auto& id : ids) {
                const auto remote = this->neighbors.get(id);
                if (remote.has_value()) {
                    descriptors.push_back(remote.value()->getPeerDescriptor());
                }
            }
            return descriptors;
        };
        return {toDescriptors(failedIds), toDescriptors(successfulIds)};
    }

    [[nodiscard]] bool hasConnection(
        const DhtAddress& nodeId,
        std::optional<ProxyDirection> direction = std::nullopt) const {
        std::scoped_lock lock(this->mutex);
        return this->connections.contains(nodeId) &&
            (!direction.has_value() ||
             this->connections.at(nodeId).direction == direction);
    }

    [[nodiscard]] std::optional<ProxyDirection> getDirection() const {
        std::scoped_lock lock(this->mutex);
        return this->definition->direction;
    }

    [[nodiscard]] EthereumAddress getLocalEthereumAddress() const {
        return EthereumAddress{Identifiers::getNodeIdFromPeerDescriptor(
            this->options.localPeerDescriptor)};
    }

    [[nodiscard]] StreamPartID getStreamPartID() const {
        return this->options.streamPartId;
    }

    void onNodeDisconnected(const PeerDescriptor& peerDescriptor) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        {
            std::scoped_lock lock(this->mutex);
            if (this->stopped || this->connections.erase(nodeId) == 0) {
                return;
            }
        }
        this->options.connectionLocker.unlockConnection(
            peerDescriptor, LockID{SERVICE_ID});
        this->neighbors.remove(nodeId);
        // Deviation from TS: no automatic reconnection attempt here. The
        // TS single retry always fails in practice (the proxy that
        // disconnected is gone or has left the stream part), and issuing
        // blocking RPCs from transport event threads (this handler runs
        // on websocket/rtc threads) deadlocks against the in-flight
        // sends those threads service. Callers reconnect by re-issuing
        // setProxies() — see the reconnect end-to-end test.
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
        {
            std::scoped_lock lock(this->mutex);
            if (this->stopped) {
                return;
            }
            this->stopped = true;
            this->connections.clear();
        }
        // The blocking leave notices run OUTSIDE the mutex: a transport
        // event thread parked on it may be the thread these sends need.
        auto allNeighbors = this->neighbors.getAll();
        for (const auto& remote : allNeighbors) {
            this->options.connectionLocker.unlockConnection(
                remote->getPeerDescriptor(), LockID{SERVICE_ID});
            streamr::utils::blockingWait(remote->leaveStreamPartNotice(
                this->options.streamPartId, false));
        }

        this->neighbors.stop();
        this->rpcCommunicator.destroy();
        this->abortController.abort();
        if (this->transportDisconnectedHandlerToken.has_value()) {
            this->options.transport
                .off<dht::transport::transportevents::Disconnected>(
                    this->transportDisconnectedHandlerToken.value());
        }
    }
};

} // namespace streamr::trackerlessnetwork::proxy
