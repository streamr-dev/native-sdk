#ifndef STREAMR_DHT_CONNECTION_CONNECTIONMANAGER_HPP
#define STREAMR_DHT_CONNECTION_CONNECTIONMANAGER_HPP

#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <folly/experimental/coro/BlockingWait.h>
#include <folly/experimental/coro/Task.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/ConnectionLockRpcLocal.hpp"
#include "streamr-dht/connection/ConnectionLockRpcRemote.hpp"
#include "streamr-dht/connection/ConnectionLocker.hpp"
#include "streamr-dht/connection/ConnectionsView.hpp"
#include "streamr-dht/connection/ConnectorFacade.hpp"
#include "streamr-dht/connection/endpoint/Endpoint.hpp"
#include "streamr-dht/dht/routing/DuplicateDetector.hpp"
#include "streamr-dht/helpers/Errors.hpp"
#include "streamr-dht/helpers/Offerer.hpp"
#include "streamr-dht/transport/RoutingRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-logger/SLogger.hpp"
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
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::endpoint::Endpoint;
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
using streamr::logger::SLogger;
using streamr::utils::waitForEvent;

namespace endpointevents = streamr::dht::connection::endpoint::endpointevents;

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
    DuplicateDetector duplicateMessageDetector;

    std::atomic<ConnectionManagerState> state = ConnectionManagerState::IDLE;
    std::map<DhtAddress, std::shared_ptr<Endpoint>> endpoints;
    std::recursive_mutex endpointsMutex;

    void addEndpoint(
        const std::shared_ptr<IPendingConnection>& pendingConnection) {
        SLogger::debug("ConnectionManager::addEndpoint start");

        auto peerDescriptor = pendingConnection->getPeerDescriptor();
        auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);

        auto endpoint = Endpoint::newInstance(
            peerDescriptor, [this, peerDescriptor, nodeId]() {
                {
                    SLogger::debug(
                        "Trying to acquire mutex lock in endpoint callback");
                    std::scoped_lock lock(this->endpointsMutex);
                    SLogger::debug("Acquired mutex lock in endpoint callback");
                    if (this->endpoints.find(nodeId) != this->endpoints.end()) {
                        this->endpoints.erase(nodeId);
                    }
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

        {
            SLogger::debug("Trying to acquire mutex lock in addEndpoint");
            std::scoped_lock lock(this->endpointsMutex);
            SLogger::debug("Acquired mutex lock in addEndpoint");
            this->endpoints[nodeId] = endpoint;
        }
        endpoint->changeToConnectingState(pendingConnection);
        SLogger::debug("ConnectionManager::addEndpoint end");
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
                  SLogger::trace(
                      "outgoingmessagecallback() of rpcCommunicator");
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
                      SLogger::debug("closeConnection() callback of RpcLocal");
                      this->closeConnection(
                          peerDescriptor, gracefulLeave, reason);
                  },
              .getLocalPeerDescriptor =
                  [this]() { return this->getLocalPeerDescriptor(); }}) {
        SLogger::debug("ConnectionManager constructor start");
        SLogger::info("ConnectionManager constructor");
        this->connectorFacade = this->options.createConnectorFacade();

        this->rpcCommunicator.registerRpcMethod<LockRequest, LockResponse>(
            "lockRequest",
            [this](const LockRequest& req, const DhtCallContext& context) {
                return this->connectionLockRpcLocal.lockRequest(req, context);
            });
        this->rpcCommunicator.registerRpcNotification<UnlockRequest>(
            "unlockRequest",
            [this](const UnlockRequest& req, const DhtCallContext& context) {
                return this->connectionLockRpcLocal.unlockRequest(req, context);
            });
        this->rpcCommunicator.registerRpcNotification<DisconnectNotice>(
            "gracefulDisconnect",
            [this](const DisconnectNotice& req, const DhtCallContext& context) {
                return this->connectionLockRpcLocal.gracefulDisconnect(
                    req, context);
            });
        SLogger::debug("ConnectionManager constructor end");
    }

    ~ConnectionManager() override {
        SLogger::debug("~ConnectionManager() start");
        SLogger::trace("~ConnectionManager()");
        this->stop();
        SLogger::debug("~ConnectionManager() end");
    };

    // garbageCollectConnections() not implemented in native-sdk

    void start() {
        SLogger::debug("ConnectionManager::start() start");
        if (this->state == ConnectionManagerState::RUNNING ||
            this->state == ConnectionManagerState::STOPPED) {
            throw CouldNotStart(
                "Cannot start already started module ConnectionManager");
        }
        this->state = ConnectionManagerState::RUNNING;

        SLogger::trace("Starting ConnectionManager...");
        this->connectorFacade->start(
            [this](const std::shared_ptr<IPendingConnection>& connection) {
                return this->onNewConnection(connection);
            },
            [this](const DhtAddress& nodeId) {
                return this->hasConnection(nodeId);
            });

        // Garbage collection of connections not implemented in native-sdk
        SLogger::debug("ConnectionManager::start() end");
    }

    void stop() override {
        SLogger::debug("ConnectionManager::stop() start");
        std::map<DhtAddress, std::shared_ptr<Endpoint>> endpointsCopy;
        {
            if (this->state == ConnectionManagerState::STOPPED ||
                this->state == ConnectionManagerState::STOPPING) {
                SLogger::debug("ConnectionManager::stop() end (early return)");
                return;
            }
            this->state = ConnectionManagerState::STOPPING;
            SLogger::trace("Stopping ConnectionManager");

            {
                SLogger::debug("Trying to acquire mutex lock in stop");
                std::scoped_lock lock(this->endpointsMutex);
                SLogger::debug("Acquired mutex lock in stop");
                // make temporary copy of endpoints to avoid iterator
                // invalidation
                endpointsCopy = this->endpoints;
            }
        }
        // pop one by one from copy to avoid iterator invalidation

        while (!endpointsCopy.empty()) {
            auto it = endpointsCopy.begin();
            auto peerDescriptorToDisconnect = it->second->getPeerDescriptor();
            endpointsCopy.erase(it);
            this->gracefullyDisconnect(
                std::move(peerDescriptorToDisconnect), DisconnectMode::LEAVING);
        }

        this->connectorFacade->stop();
        this->state = ConnectionManagerState::STOPPED;
        // this->rpcCommunicator.stop();
        this->duplicateMessageDetector.clear();
        this->locks.clear();
        this->removeAllListeners();
        SLogger::debug("ConnectionManager::stop() end");
    }

    void send(const Message& message, const SendOptions& sendOptions) override {
        SLogger::debug("ConnectionManager::send() start");

        SLogger::trace("send()");
        SLogger::debug("Traced send() function entry");
        if ((this->state == ConnectionManagerState::STOPPED ||
             this->state == ConnectionManagerState::STOPPING) &&
            !sendOptions.sendIfStopped) {
            SLogger::debug("Returning early due to stopped state");
            SLogger::debug("ConnectionManager::send() end (early return)");
            return;
        }
        SLogger::debug("Passed state check");
        const auto& peerDescriptor = message.targetdescriptor();
        SLogger::debug("Retrieved peer descriptor");
        if (this->isConnectionToSelf(peerDescriptor)) {
            SLogger::debug("Detected connection to self, throwing exception");
            throw CannotConnectToSelf("Cannot send to self");
        }
        SLogger::debug("Passed self-connection check");

        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        SLogger::debug("Retrieved node ID");
        SLogger::trace("Sending message to: " + nodeId);
        SLogger::debug("Traced sending message to node");

        Message messageWithSource = message;
        SLogger::debug("Created message with source");
        messageWithSource.mutable_sourcedescriptor()->CopyFrom(
            this->getLocalPeerDescriptor());
        SLogger::debug("Copied local peer descriptor to message");

        const auto debugMessage = messageWithSource.DebugString();
        SLogger::debug("Created debug message string");
        SLogger::trace("Sending message: " + debugMessage);
        SLogger::debug("Traced sending message details");

        std::shared_ptr<Endpoint> endpoint;
        {
            SLogger::debug("Trying to acquire mutex lock in send");
            std::scoped_lock lock(this->endpointsMutex);
            SLogger::debug("Acquired mutex lock in send");

            if (this->endpoints.find(nodeId) == this->endpoints.end()) {
                SLogger::debug("Node ID not found in endpoints");
                if (sendOptions.connect) {
                    SLogger::debug("Creating new connection");
                    std::shared_ptr<IPendingConnection> connection =
                        this->connectorFacade->createConnection(peerDescriptor);

                    SLogger::debug("Created new connection");
                    this->onNewConnection(connection);
                    SLogger::debug("Handled new connection");
                    if (this->endpoints.find(nodeId) == this->endpoints.end()) {
                        SLogger::debug(
                            "Node ID not found in endpoints after creating new connection, this means that the connection failed");
                        throw SendFailed(
                            "No connection to target, connection failed");
                    }
                } else {
                    SLogger::debug("Throwing SendFailed exception");
                    throw SendFailed(
                        "No connection to target, connect flag is false");
                }
            } else if (
                !this->endpoints.at(nodeId)->isConnected() &&
                !sendOptions.connect) {
                SLogger::debug(
                    "Throwing SendFailed exception due to disconnected endpoint");
                throw SendFailed(
                    "No connection to target, connect flag is false");
            }
            endpoint = this->endpoints.at(nodeId);
            SLogger::debug("Passed connection checks");
        }
        size_t nBytes = messageWithSource.ByteSizeLong();
        SLogger::debug("Calculated message size");
        if (nBytes == 0) {
            SLogger::debug("Message size is zero, throwing exception");
            SLogger::error("send(): serialized message is empty");
            throw SendFailed("send(): serialized message is empty");
        }
        SLogger::debug("Passed message size check");
        std::vector<std::byte> byteVec(nBytes);
        SLogger::debug("Created byte vector");
        messageWithSource.SerializeToArray(
            byteVec.data(), static_cast<int>(nBytes));
        SLogger::debug("Serialized message to byte vector");
        endpoint->send(byteVec);
        SLogger::debug("Sent message through endpoint");
        SLogger::debug("ConnectionManager::send() end");
    }

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const override {
        SLogger::debug("ConnectionManager::getLocalPeerDescriptor() start");
        auto result = this->connectorFacade->getLocalPeerDescriptor();
        SLogger::debug("ConnectionManager::getLocalPeerDescriptor() end");
        return result;
    }

    [[nodiscard]] bool hasConnection(const DhtAddress& nodeId) override {
        SLogger::debug("ConnectionManager::hasConnection() start");
        auto result = std::ranges::any_of(
            this->getConnections(), [&nodeId](const auto& c) {
                return Identifiers::getNodeIdFromPeerDescriptor(c) == nodeId;
            });
        SLogger::debug("ConnectionManager::hasConnection() end");
        return result;
    }

    [[nodiscard]] size_t getConnectionCount() override {
        SLogger::debug("ConnectionManager::getConnectionCount() start");
        auto result = this->getConnections().size();
        SLogger::debug("ConnectionManager::getConnectionCount() end");
        return result;
    }

    [[nodiscard]] bool hasLocalLockedConnection(const DhtAddress& nodeId) {
        SLogger::debug("ConnectionManager::hasLocalLockedConnection() start");
        auto result = this->locks.isLocalLocked(nodeId);
        SLogger::debug("ConnectionManager::hasLocalLockedConnection() end");
        return result;
    }

    [[nodiscard]] bool hasRemoteLockedConnection(const DhtAddress& nodeId) {
        return this->locks.isRemoteLocked(nodeId);
    }

    void lockConnection(
        PeerDescriptor targetDescriptor, LockID lockId) override {
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
            auto accepted = folly::coro::blockingWait(
                rpcRemote.lockRequest(std::move(lockId)));
            if (accepted) {
                SLogger::trace("LockRequest successful");
            } else {
                SLogger::debug("LockRequest failed");
            }
        } catch (const std::exception& err) {
            SLogger::error(
                "Caught exception when making lockRequest " +
                std::string(err.what()));
        }
    }

    void unlockConnection(
        PeerDescriptor targetDescriptor, LockID lockId) override {
        if (this->state == ConnectionManagerState::STOPPED ||
            Identifiers::areEqualPeerDescriptors(
                targetDescriptor, this->getLocalPeerDescriptor())) {
            return;
        }

        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetDescriptor);
        this->locks.removeLocalLocked(nodeId, lockId);

        {
            SLogger::debug("Trying to acquire mutex lock in unlockConnection");
            std::scoped_lock lock(this->endpointsMutex);
            SLogger::debug("Acquired mutex lock in unlockConnection");
            if (this->endpoints.find(nodeId) == this->endpoints.end()) {
                SLogger::debug("Node ID not found in endpoints");
                return;
            }
        }

        ConnectionLockRpcClient client{this->rpcCommunicator};
        ConnectionLockRpcRemote rpcRemote(
            this->getLocalPeerDescriptor(), targetDescriptor, client);

        folly::coro::blockingWait(rpcRemote.unlockRequest(std::move(lockId)));
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

    [[nodiscard]] size_t getLocalLockedConnectionCount() override {
        return this->locks.getLocalLockedConnectionCount();
    }

    [[nodiscard]] size_t getRemoteLockedConnectionCount() override {
        return this->locks.getRemoteLockedConnectionCount();
    }

    [[nodiscard]] size_t getWeakLockedConnectionCount() override {
        return this->locks.getWeakLockedConnectionCount();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getConnections() override {
        std::scoped_lock lock(this->endpointsMutex);
        return this->endpoints | std::views::values |
            std::views::filter([](const auto& endpoint) {
                   return endpoint->isConnected();
               }) |
            std::views::transform([](const auto& endpoint) {
                   return endpoint->getPeerDescriptor();
               }) |
            std::ranges::to<std::vector>();
    }

private:
    bool onNewConnection(const std::shared_ptr<IPendingConnection>& connection) {
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
        const std::shared_ptr<IPendingConnection>& newConnection) {
        const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
            newConnection->getPeerDescriptor());
        SLogger::debug("ConnectionManager::acceptNewConnection()");

        {
            SLogger::debug("ConnectionManager::acceptNewConnection() start");
            std::scoped_lock lock(this->endpointsMutex);
            SLogger::debug("Acquired mutex lock in acceptNewConnection");

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
        }
        SLogger::debug("Calling addEndpoint() in acceptNewConnection()");
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

        std::shared_ptr<Endpoint> endpoint;
        {
            SLogger::debug("ConnectionManager::closeConnection() start");
            std::scoped_lock lock(this->endpointsMutex);
            SLogger::debug("Acquired mutex lock in closeConnection");
            auto it = this->endpoints.find(nodeId);
            if (it != this->endpoints.end()) {
                endpoint = it->second;
            }
        }

        if (endpoint) {
            endpoint->close(gracefulLeave);
        } else {
            SLogger::trace(
                nodeId +
                " closeConnection() this.endpoints did not have the id");
            this->emit<Disconnected>(peerDescriptor, false);
        }
    }

    void gracefullyDisconnect(
        PeerDescriptor&& targetDescriptor, DisconnectMode&& disconnectMode) {
        SLogger::debug("ConnectionManager::gracefullyDisconnect() start");

        std::shared_ptr<Endpoint> endpoint = nullptr;
        {
            SLogger::debug(
                "Trying to acquire mutex lock in gracefullyDisconnect");
            std::unique_lock lock(this->endpointsMutex);
            SLogger::debug("Acquired mutex lock in gracefullyDisconnect");
            auto it = this->endpoints.find(
                Identifiers::getNodeIdFromPeerDescriptor(targetDescriptor));
            if (it != this->endpoints.end()) {
                endpoint = it->second;
            }
        }
        if (endpoint == nullptr) {
            SLogger::debug(
                "gracefullyDisconnected() tried on a non-existing connection");
            return;
        }
        auto debugString = targetDescriptor.DebugString();

        if (endpoint->isConnected()) {
            try {
                SLogger::debug("gracefullyDisconnect() calling blockingWait()");
                folly::coro::blockingWait(folly::coro::co_invoke(
                    [this,
                     endpoint,
                     targetDescriptor = std::move(targetDescriptor),
                     disconnectMode]() -> folly::coro::Task<void> {
                        co_await folly::coro::collectAll(
                            waitForEvent<endpointevents::Disconnected>(
                                endpoint.get(), 2000ms), // NOLINT
                            folly::coro::co_invoke(
                                [this,
                                 endpoint,
                                 targetDescriptor,
                                 disconnectMode]() -> folly::coro::Task<void> {
                                    auto debugString =
                                        targetDescriptor.DebugString();

                                    co_return co_await this
                                        ->doGracefullyDisconnectAsync(
                                            targetDescriptor, disconnectMode);
                                }));
                    }));
            } catch (const std::exception& err) {
                SLogger::error(
                    "Caught exception in gracefullyDisconnect " +
                    std::string(err.what()) + "\n");
                endpoint->close(true);
            }
            SLogger::debug("gracefullyDisconnect() blockingWait() returned");
        } else {
            SLogger::debug(
                "gracefullyDisconnected() failed, force-closing endpoint");
            endpoint->close(true);
        }
        SLogger::debug("ConnectionManager::gracefullyDisconnect() end");
    }

    folly::coro::Task<void> doGracefullyDisconnectAsync(
        PeerDescriptor targetDescriptor, DisconnectMode disconnectMode) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetDescriptor);
        SLogger::trace(nodeId + " gracefullyDisconnectAsync()");

        ConnectionLockRpcClient client{this->rpcCommunicator};
        ConnectionLockRpcRemote rpcRemote(
            this->getLocalPeerDescriptor(), targetDescriptor, client);

        co_return co_await rpcRemote.gracefulDisconnect(
            std::forward<DisconnectMode>(disconnectMode));
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