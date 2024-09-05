#ifndef STREAMR_DHT_CONNECTION_CONNECTIONMANAGER_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONMANAGER_HPP

#include <chrono>
#include <memory>
#include <map>
#include <folly/coro/Task.h>
#include <folly/coro/blockingWait.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/ConnectionLockRpcLocal.hpp"
#include "streamr-dht/connection/ConnectionLockRpcRemote.hpp"
#include "streamr-dht/connection/ConnectionLocker.hpp"
#include "streamr-dht/connection/ConnectionsView.hpp"
#include "streamr-dht/connection/ConnectorFacade.hpp"
#include "streamr-dht/connection/Endpoint.hpp"
#include "streamr-dht/dht/routing/DuplicateDetector.hpp"
#include "streamr-dht/helpers/Errors.hpp"
#include "streamr-dht/helpers/Offerer.hpp"
#include "streamr-dht/transport/RoutingRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-utils/waitForEvent.hpp"

namespace streamr::dht::connection {

using ::dht::LockRequest;
using ::dht::LockResponse;
using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::UnlockRequest;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::ConnectionLockRpcLocal;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::connection::Endpoint;
using streamr::dht::connection::PendingConnection;
using streamr::dht::helpers::CannotConnectToSelf;
using streamr::dht::helpers::CouldNotStart;
using streamr::dht::helpers::Offerer;
using streamr::dht::helpers::OffererHelper;
using streamr::dht::helpers::SendFailed;
using streamr::dht::routing::DuplicateDetector;
using streamr::dht::transport::RoutingRpcCommunicator;
using streamr::dht::transport::SendOptions;
using streamr::dht::transport::Transport;
using streamr::dht::transport::transportevents::Disconnected;
using streamr::utils::waitForEvent;

using namespace std::chrono_literals;

enum class ConnectionManagerState { IDLE, RUNNING, STOPPING, STOPPED };

struct ConnectionManagerOptions {
    size_t maxConnections;
    // MetricsContext metricsContext;
    std::function<std::shared_ptr<ConnectorFacade>()> createConnectorFacade;
};

class ConnectionManager : public Transport,
                          public ConnectionsView,
                          public ConnectionLocker {
private:
    ConnectionManagerOptions options;
    std::shared_ptr<ConnectorFacade> connectorFacade;
    RoutingRpcCommunicator rpcCommunicator;
    ConnectionLockStates locks;
    ConnectionLockRpcLocal connectionLockRpcLocal;
    std::map<DhtAddress, std::shared_ptr<Endpoint>> endpoints;
    ConnectionManagerState state = ConnectionManagerState::IDLE;
    DuplicateDetector duplicateMessageDetector;

    void addEndpoint(std::shared_ptr<PendingConnection> pendingConnection) {
        auto peerDescriptor = pendingConnection->getPeerDescriptor();
        auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        auto endpoint = std::make_shared<Endpoint>(
            pendingConnection, [this, peerDescriptor, nodeId]() {
                if (this->endpoints.find(nodeId) != this->endpoints.end()) {
                    this->endpoints.erase(nodeId);
                }
                this->emit<Disconnected>(peerDescriptor, true);
            });

        endpoint->on<endpointevents::Data>(
            [this, peerDescriptor](const std::vector<std::byte>& data) {
                this->onData(data, peerDescriptor);
            });

        endpoint->on<endpointevents::Connected>([this, peerDescriptor]() {
            this->emit<transport::transportevents::Connected>(peerDescriptor);
        });

        this->endpoints[nodeId] = std::move(endpoint);
    }

public:
    // NOLINTNEXTLINE
    static constexpr auto INTERNAL_SERVICE_ID = "system/connection-manager";
    // NOLINTNEXTLINE
    static constexpr auto DUPLICATE_DETECTOR_SIZE = 10000;

    explicit ConnectionManager(ConnectionManagerOptions&& options)
        : options(std::move(options)),
          duplicateMessageDetector(DUPLICATE_DETECTOR_SIZE),
          rpcCommunicator(
              ServiceID{INTERNAL_SERVICE_ID},
              [this](const Message& message, const SendOptions& sendOptions) {
                  return this->send(message, sendOptions);
              },
              RpcCommunicatorOptions{.rpcRequestTimeout = 10s}), // NOLINT
          connectionLockRpcLocal(ConnectionLockRpcLocalOptions{
              .addRemoteLocked =
                  [this](const DhtAddress& id, const LockID& lockId) {
                      this->locks.addRemoteLocked(id, lockId);
                  },
              .removeRemoteLocked =
                  [this](const DhtAddress& id, const LockID& lockId) {
                      this->locks.removeRemoteLocked(id, lockId);
                  },
              .closeConnection =
                  [this](
                      const PeerDescriptor& peerDescriptor,
                      bool gracefulLeave,
                      const std::optional<std::string>& reason) {
                      this->closeConnection(
                          peerDescriptor, gracefulLeave, reason);
                  },
              .getLocalPeerDescriptor =
                  [this]() { return this->getLocalPeerDescriptor(); }}) {
        this->connectorFacade = options.createConnectorFacade();
        this->rpcCommunicator.registerRpcMethod<LockRequest, LockResponse>(
            "lockRequest",
            [this](const LockRequest& req, const ProtoCallContext& context) {
                return this->connectionLockRpcLocal.lockRequest(req, context);
            });
        this->rpcCommunicator.registerRpcNotification<UnlockRequest>(
            "unlockRequest",
            [this](const UnlockRequest& req, const ProtoCallContext& context) {
                return this->connectionLockRpcLocal.unlockRequest(req, context);
            });
        this->rpcCommunicator.registerRpcNotification<DisconnectNotice>(
            "gracefulDisconnect",
            [this](
                const DisconnectNotice& req, const ProtoCallContext& context) {
                return this->connectionLockRpcLocal.gracefulDisconnect(
                    req, context);
            });
    }

    ~ConnectionManager() override = default;

    // garbageCollectConnections() not implemented in native-sdk

    void start() {
        if (this->state == ConnectionManagerState::RUNNING ||
            this->state == ConnectionManagerState::STOPPED) {
            throw CouldNotStart(
                "Cannot start already started module ConnectionManager");
        }
        this->state = ConnectionManagerState::RUNNING;

        SLogger::trace("Starting ConnectionManager...");
        this->connectorFacade->start(
            [this](const std::shared_ptr<PendingConnection>& connection) {
                return this->onNewConnection(connection);
            },
            [this](const DhtAddress& nodeId) {
                return this->hasConnection(nodeId);
            });

        // Garbage collection of connections not implemented in native-sdk
    }

    void send(const Message& message, const SendOptions& sendOptions) override {
        if ((this->state == ConnectionManagerState::STOPPED ||
             this->state == ConnectionManagerState::STOPPING) &&
            !sendOptions.sendIfStopped) {
            return;
        }
        const auto& peerDescriptor = message.targetdescriptor();
        if (this->isConnectionToSelf(peerDescriptor)) {
            throw CannotConnectToSelf("Cannot send to self");
        }

        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        SLogger::trace("Sending message to: " + nodeId);

        Message messageWithSource = message;
        messageWithSource.mutable_sourcedescriptor()->CopyFrom(
            this->getLocalPeerDescriptor());

        if (this->endpoints.find(nodeId) == this->endpoints.end()) {
            if (sendOptions.connect) {
                auto connection =
                    this->connectorFacade->createConnection(peerDescriptor);
                this->onNewConnection(connection);
            } else {
                throw SendFailed(
                    "No connection to target, connect flag is false");
            }
        } else if (
            !this->endpoints.at(nodeId)->isConnected() &&
            !sendOptions.connect) {
            throw SendFailed("No connection to target, connect flag is false");
        }

        size_t nBytes = messageWithSource.ByteSizeLong();
        if (nBytes == 0) {
            SLogger::error("send(): serialized message is empty");
            throw SendFailed("send(): serialized message is empty");
        }
        std::vector<std::byte> byteVec(nBytes);
        messageWithSource.SerializeToArray(
            byteVec.data(), static_cast<int>(nBytes));
        this->endpoints.at(nodeId)->send(byteVec);
    }

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const {
        return this->connectorFacade->getLocalPeerDescriptor();
    }

    [[nodiscard]] bool hasConnection(const DhtAddress& nodeId) const override {
        return std::ranges::any_of(
            this->getConnections(), [&nodeId](const auto& c) {
                return Identifiers::getNodeIdFromPeerDescriptor(c) == nodeId;
            });
    }

    [[nodiscard]] size_t getConnectionCount() const override {
        return this->getConnections().size();
    }

    [[nodiscard]] bool hasLocalLockedConnection(
        const DhtAddress& nodeId) const {
        return this->locks.isLocalLocked(nodeId);
    }

    [[nodiscard]] bool hasRemoteLockedConnection(
        const DhtAddress& nodeId) const {
        return this->locks.isRemoteLocked(nodeId);
    }

    void lockConnection(
        const PeerDescriptor& targetDescriptor, const LockID& lockId) override {
        if (this->state == ConnectionManagerState::STOPPED ||
            Identifiers::areEqualPeerDescriptors(
                targetDescriptor, this->getLocalPeerDescriptor())) {
            return;
        }
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetDescriptor);

        ConnectionLockRpcClient client{this->rpcCommunicator};
        ConnectionLockRpcRemote rpcRemote(
            this->getLocalPeerDescriptor(), targetDescriptor, client);

        this->locks.addLocalLocked(nodeId, lockId);

        try {
            auto accepted =
                folly::coro::blockingWait(rpcRemote.lockRequest(lockId));
            if (accepted) {
                SLogger::trace("LockRequest successful");
            } else {
                SLogger::debug("LockRequest failed");
            }
        } catch (const std::exception& err) {
            SLogger::error("Caught exception when making lockRequest " + std::string(err.what()));
        }
    }

    void unlockConnection(
        const PeerDescriptor& targetDescriptor, const LockID& lockId) override {
        if (this->state == ConnectionManagerState::STOPPED ||
            Identifiers::areEqualPeerDescriptors(
                targetDescriptor, this->getLocalPeerDescriptor())) {
            return;
        }

        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetDescriptor);
        this->locks.removeLocalLocked(nodeId, lockId);

        if (this->endpoints.find(nodeId) != this->endpoints.end()) {
            ConnectionLockRpcClient client{this->rpcCommunicator};
            ConnectionLockRpcRemote rpcRemote(
                this->getLocalPeerDescriptor(), targetDescriptor, client);

            folly::coro::blockingWait(rpcRemote.unlockRequest(lockId));
        }
    }

    void weakLockConnection(
        const DhtAddress& nodeId, const LockID& lockId) override {
        if (this->state == ConnectionManagerState::STOPPED ||
            (nodeId ==
             Identifiers::getNodeIdFromPeerDescriptor(
                 this->getLocalPeerDescriptor()))) {
            return;
        }
        this->locks.addWeakLocked(nodeId, lockId);
    }

    void weakUnlockConnection(
        const DhtAddress& nodeId, const LockID& lockId) override {
        if (this->state == ConnectionManagerState::STOPPED ||
            (nodeId ==
             Identifiers::getNodeIdFromPeerDescriptor(
                 this->getLocalPeerDescriptor()))) {
            return;
        }
        this->locks.removeWeakLocked(nodeId, lockId);
    }

    [[nodiscard]] size_t getLocalLockedConnectionCount() const override {
        return this->locks.getLocalLockedConnectionCount();
    }

    [[nodiscard]] size_t getRemoteLockedConnectionCount() const override {
        return this->locks.getRemoteLockedConnectionCount();
    }

    [[nodiscard]] size_t getWeakLockedConnectionCount() const override {
        return this->locks.getWeakLockedConnectionCount();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getConnections() const override {
        return endpoints | std::views::values |
            std::views::filter([](const auto& endpoint) {
                   return endpoint->isConnected();
               }) |
            std::views::transform([](const auto& endpoint) {
                   return endpoint->getPeerDescriptor();
               }) |
            std::ranges::to<std::vector>();
    }

private:
    bool onNewConnection(const std::shared_ptr<PendingConnection>& connection) {
        if (this->state == ConnectionManagerState::STOPPED) {
            return false;
        }
        SLogger::trace("onNewConnection()");

        return this->acceptNewConnection(connection);

        // connection.once('connected', (peerDescriptor: PeerDescriptor,
        // connection: IConnection) => this.onConnected(peerDescriptor,
        // connection)) connection.once('disconnected', (gracefulLeave: boolean)
        // => this.onDisconnected(connection.getPeerDescriptor(),
        // gracefulLeave))
        // return true;
    }

    bool acceptNewConnection(
        const std::shared_ptr<PendingConnection>& newConnection) {
        const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
            newConnection->getPeerDescriptor());
        SLogger::trace(nodeId + " acceptNewConnection()");

        if (this->endpoints.find(nodeId) != this->endpoints.end()) {
            if (OffererHelper::getOfferer(
                    Identifiers::getNodeIdFromPeerDescriptor(
                        this->getLocalPeerDescriptor()),
                    nodeId) == Offerer::REMOTE) {
                this->endpoints.at(nodeId)->setConnecting(newConnection);
                return true;
            }
            return false;
        }

        this->addEndpoint(newConnection);
        SLogger::trace(nodeId + " added to connections at acceptNewConnection");
        return true;
    }

    void closeConnection(
        const PeerDescriptor& peerDescriptor,
        bool gracefulLeave,
        const std::optional<std::string>& reason) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        SLogger::trace(nodeId + " closeConnection() " + reason.value_or(""));
        this->locks.clearAllLocks(nodeId);

        if (this->endpoints.find(nodeId) != this->endpoints.end()) {
            this->endpoints.at(nodeId)->close(gracefulLeave);
        } else {
            SLogger::trace(
                nodeId +
                " closeConnection() this.endpoints did not have the id");
            this->emit<Disconnected>(peerDescriptor, false);
        }
    }

    void gracefullyDisconnectAsync(
        const PeerDescriptor& targetDescriptor,
        const DisconnectMode& disconnectMode) {
        if (this->endpoints.find(Identifiers::getNodeIdFromPeerDescriptor(
                targetDescriptor)) == this->endpoints.end()) {
            SLogger::debug(
                "gracefullyDisconnectedAsync() tried on a non-existing connection");
            return;
        }
        const auto endpoint = this->endpoints.at(
            Identifiers::getNodeIdFromPeerDescriptor(targetDescriptor));

        if (endpoint->isConnected()) {
            try {
                folly::coro::blockingWait(folly::coro::co_invoke(
                    [this, &endpoint, &targetDescriptor, &disconnectMode]()
                        -> folly::coro::Task<void> {
                        co_await folly::coro::collectAll(
                            waitForEvent<endpointevents::Disconnected>(
                                *endpoint, 2000ms), // NOLINT
                            folly::coro::co_invoke(
                                [this,
                                 &endpoint,
                                 &targetDescriptor,
                                 &disconnectMode]() -> folly::coro::Task<void> {
                                    co_await this->doGracefullyDisconnectAsync(
                                        targetDescriptor, disconnectMode);
                                }));
                    }));
            } catch (const std::exception& err) {
                SLogger::error(
                    "Caught exception in gracefullyDisconnectAsync " + std::string(err.what()));
                endpoint->close(true);
            }
        }
    }

    folly::coro::Task<void> doGracefullyDisconnectAsync(
        const PeerDescriptor& targetDescriptor,
        const DisconnectMode disconnectMode) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetDescriptor);
        SLogger::trace(nodeId + " gracefullyDisconnectAsync()");

        ConnectionLockRpcClient client{this->rpcCommunicator};
        ConnectionLockRpcRemote rpcRemote(
            this->getLocalPeerDescriptor(), targetDescriptor, client);

        return rpcRemote.gracefulDisconnect(disconnectMode);
    }

    void handleMessage(const Message& message) {
        SLogger::trace("Received message " + message.DebugString());

        if (!message.has_rpcmessage()) {
            SLogger::trace(
                "Filtered out non-RPC message: " + message.DebugString());
            return;
        }
        if (this->duplicateMessageDetector.isMostLikelyDuplicate(
                message.messageid())) {
            SLogger::trace(
                "handleMessage filtered duplicate " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    message.sourcedescriptor()) +
                " " + message.serviceid() + " " + message.messageid());
            return;
        }

        this->duplicateMessageDetector.add(message.messageid());

        if (message.serviceid() == INTERNAL_SERVICE_ID) {
            this->rpcCommunicator.handleMessageFromPeer(message);
        } else {
            SLogger::trace(
                "emit \"message\" " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    message.sourcedescriptor()) +
                " " + message.serviceid() + " " + message.messageid());
            this->emit<transport::transportevents::Message>(message);
        }
    }

    void onData(
        const std::vector<std::byte>& data,
        const PeerDescriptor& peerDescriptor) {
        if (this->state == ConnectionManagerState::STOPPED) {
            return;
        }
        Message message;
        try {
            message.ParseFromArray(data.data(), static_cast<int>(data.size()));
        } catch (const std::exception& e) {
            SLogger::debug(
                "Parsing incoming data into Message failed: " +
                std::string(e.what()));
            return;
        }

        message.mutable_sourcedescriptor()->CopyFrom(peerDescriptor);

        try {
            this->handleMessage(message);
        } catch (const std::exception& e) {
            SLogger::debug(
                "Handling incoming data failed: " + std::string(e.what()));
        }
    }

    bool isConnectionToSelf(const PeerDescriptor& peerDescriptor) const {
        return Identifiers::areEqualPeerDescriptors(
                   peerDescriptor, this->getLocalPeerDescriptor()) ||
            this->isOwnWebsocketServer(peerDescriptor);
    }

    bool isOwnWebsocketServer(const PeerDescriptor& peerDescriptor) const {
        const auto localPeerDescriptor = this->getLocalPeerDescriptor();
        if ((peerDescriptor.has_websocket()) &&
            (localPeerDescriptor.has_websocket())) {
            return (
                (peerDescriptor.websocket().port() ==
                 localPeerDescriptor.websocket().port()) &&
                (peerDescriptor.websocket().host() ==
                 localPeerDescriptor.websocket().host()));
        }
        return false;
    }
};

} // namespace streamr::dht::connection
#endif // STREAMR_DHT_CONNECTION_CONNECTIONMANAGER_HPP