// Module streamr.dht.ConnectionManager
// CONSOLIDATED from the former header
// streamr-dht/connection/ConnectionManager.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.



export module streamr.dht.ConnectionManager;

import std;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.protorpc.RpcCommunicator;
import streamr.dht.ConnectionLockStates;
import streamr.logger.SLogger;
import streamr.utils.waitForEvent;
import streamr.dht.ConnectionLockRpcLocal;
import streamr.dht.ConnectionLockRpcRemote;
import streamr.dht.ConnectionLocker;
import streamr.dht.ConnectionsView;
import streamr.dht.ConnectorFacade;
import streamr.dht.DuplicateDetector;
import streamr.dht.Endpoint;
import streamr.dht.Errors;
import streamr.dht.Identifiers;
import streamr.dht.Offerer;
import streamr.dht.RoutingRpcCommunicator;
import streamr.dht.Transport;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
using streamr::protorpc::RpcCommunicatorOptions;
using streamr::utils::waitForEvent;

export namespace streamr::dht::connection {

using ::dht::LockRequest;
using ::dht::LockResponse;
using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::SetPrivateRequest;
using ::dht::UnlockRequest;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::ConnectionLockRpcLocal;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::PendingConnection;
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

namespace endpointevents = streamr::dht::connection::endpoint::endpointevents;

using namespace std::chrono_literals;

enum class ConnectionManagerState : std::uint8_t {
    IDLE,
    RUNNING,
    STOPPING,
    STOPPED
};

struct ConnectionManagerOptions {
    std::size_t maxConnections;
    // MetricsContext metricsContext;
    std::function<std::shared_ptr<ConnectorFacade>()> createConnectorFacade;
    // Whether remote peers may mark their connection to us as private
    // (setPrivate RPC); mirrors the TS option of the same name.
    bool allowIncomingPrivateConnections = false;
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
    // Monotonic counter handed to Endpoint::setConnecting so an endpoint
    // adopts pending connections in tie-break decision order even when the
    // setConnecting() calls race across threads; guarded by endpointsMutex.
    // See acceptNewConnection().
    std::uint64_t connectingSequenceCounter = 0;

    // Constructs an Endpoint for the peer and wires its listeners; pure
    // construction with no call-outs, so acceptNewConnection() may run
    // it while holding endpointsMutex. The caller inserts the endpoint
    // into the container and starts it connecting.
    [[nodiscard]] std::shared_ptr<Endpoint> createEndpoint(
        const PeerDescriptor& peerDescriptor, const DhtAddress& nodeId) {
        SLogger::debug("ConnectionManager::createEndpoint start");

        auto endpoint = Endpoint::newInstance(
            peerDescriptor, [this, peerDescriptor, nodeId]() {
                {
                    SLogger::debug(
                        "Trying to acquire mutex lock in endpoint callback");
                    std::scoped_lock lock(this->endpointsMutex);
                    SLogger::debug("Acquired mutex lock in endpoint callback");
                    if (this->endpoints.contains(nodeId)) {
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

        SLogger::debug("ConnectionManager::createEndpoint end");
        return endpoint;
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
                  this->send(message, sendOptions);
              },
              RpcCommunicatorOptions{.rpcRequestTimeout = 10s}), // NOLINT
          connectionLockRpcLocal(
              ConnectionLockRpcLocalOptions{
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
                          SLogger::debug(
                              "closeConnection() callback of RpcLocal");
                          this->closeConnection(
                              peerDescriptor, gracefulLeave, reason);
                      },
                  .getLocalPeerDescriptor =
                      [this]() { return this->getLocalPeerDescriptor(); },
                  .setPrivate =
                      [this](const DhtAddress& id, bool isPrivate) {
                          if (!this->options.allowIncomingPrivateConnections) {
                              SLogger::debug(
                                  "node " + id +
                                  " attempted to set a connection as private,"
                                  " but it is not allowed");
                              return;
                          }
                          if (isPrivate) {
                              this->locks.addPrivate(id);
                          } else {
                              this->locks.removePrivate(id);
                          }
                      }}) {
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
                this->connectionLockRpcLocal.unlockRequest(req, context);
            });
        this->rpcCommunicator.registerRpcNotification<DisconnectNotice>(
            "gracefulDisconnect",
            [this](const DisconnectNotice& req, const DhtCallContext& context) {
                this->connectionLockRpcLocal.gracefulDisconnect(req, context);
            });
        this->rpcCommunicator.registerRpcNotification<SetPrivateRequest>(
            "setPrivate",
            [this](
                const SetPrivateRequest& req, const DhtCallContext& context) {
                this->connectionLockRpcLocal.setPrivate(req, context);
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
                // Connections handed to us by the connector are incoming,
                // i.e. initiated by the remote peer.
                return this->onNewConnection(
                    connection, /*isLocalInitiated=*/false);
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

        // endpointsMutex is held only for the container lookups;
        // createConnection/onNewConnection and the endpoint queries lead
        // into the connector facade and the endpoint state machine, and
        // holding the container lock across them nests it with the
        // endpoint mutex (phase-A0 locking policy: no call-outs under
        // endpointsMutex).
        std::shared_ptr<Endpoint> endpoint;
        {
            SLogger::debug("Trying to acquire mutex lock in send");
            std::scoped_lock lock(this->endpointsMutex);
            SLogger::debug("Acquired mutex lock in send");
            const auto it = this->endpoints.find(nodeId);
            if (it != this->endpoints.end()) {
                endpoint = it->second;
            }
        }
        if (!endpoint) {
            SLogger::debug("Node ID not found in endpoints");
            if (!sendOptions.connect) {
                SLogger::debug("Throwing SendFailed exception");
                throw SendFailed(
                    "No connection to target, connect flag is false");
            }
            SLogger::debug("Creating new connection");
            std::shared_ptr<IPendingConnection> connection =
                this->connectorFacade->createConnection(peerDescriptor);
            SLogger::debug("Created new connection");
            // This connection is outgoing (locally initiated). If the
            // tie-break rejects it — a concurrent incoming connection from
            // the same peer already won and owns the endpoint — discard the
            // outgoing one we just created and fall through to that
            // endpoint. (Simultaneous connect; see acceptNewConnection.)
            const bool accepted =
                this->onNewConnection(connection, /*isLocalInitiated=*/true);
            SLogger::debug("Handled new connection");
            if (!accepted) {
                connection->close(false);
            }
            {
                std::scoped_lock lock(this->endpointsMutex);
                const auto it = this->endpoints.find(nodeId);
                if (it != this->endpoints.end()) {
                    endpoint = it->second;
                }
            }
            if (!endpoint) {
                SLogger::debug(
                    "Node ID not found in endpoints after creating new connection, this means that the connection failed");
                throw SendFailed("No connection to target, connection failed");
            }
        } else if (!endpoint->isConnected() && !sendOptions.connect) {
            SLogger::debug(
                "Throwing SendFailed exception due to disconnected endpoint");
            throw SendFailed("No connection to target, connect flag is false");
        }
        SLogger::debug("Passed connection checks");
        std::size_t nBytes = messageWithSource.ByteSizeLong();
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

    [[nodiscard]] std::size_t getConnectionCount() override {
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
            auto accepted = streamr::utils::blockingWait(
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
            if (!this->endpoints.contains(nodeId)) {
                SLogger::debug("Node ID not found in endpoints");
                return;
            }
        }

        ConnectionLockRpcClient client{this->rpcCommunicator};
        ConnectionLockRpcRemote rpcRemote(
            this->getLocalPeerDescriptor(), targetDescriptor, client);

        streamr::utils::blockingWait(
            rpcRemote.unlockRequest(std::move(lockId)));
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

    [[nodiscard]] std::size_t getLocalLockedConnectionCount() override {
        return this->locks.getLocalLockedConnectionCount();
    }

    [[nodiscard]] std::size_t getRemoteLockedConnectionCount() override {
        return this->locks.getRemoteLockedConnectionCount();
    }

    [[nodiscard]] std::size_t getWeakLockedConnectionCount() override {
        return this->locks.getWeakLockedConnectionCount();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getConnections() override {
        // Snapshot the endpoints under the container lock, query them
        // after releasing it: isConnected() takes the endpoint
        // state-machine mutex, and nesting it under endpointsMutex is
        // against the phase-A0 locking policy.
        std::vector<std::shared_ptr<Endpoint>> endpointsSnapshot;
        {
            std::scoped_lock lock(this->endpointsMutex);
            endpointsSnapshot = this->endpoints | std::views::values |
                std::ranges::to<std::vector>();
        }
        return endpointsSnapshot | std::views::filter([](const auto& endpoint) {
                   return endpoint->isConnected();
               }) |
            std::views::transform([](const auto& endpoint) {
                   return endpoint->getPeerDescriptor();
               }) |
            std::ranges::to<std::vector>();
    }

private:
    // isLocalInitiated: true for a connection we created ourselves
    // (send() -> createConnection, outgoing), false for one handed to us
    // by the connector (incoming, initiated by the remote peer).
    bool onNewConnection(
        const std::shared_ptr<IPendingConnection>& connection,
        bool isLocalInitiated) {
        if (this->state == ConnectionManagerState::STOPPED) {
            return false;
        }
        SLogger::trace("onNewConnection()");

        return this->acceptNewConnection(connection, isLocalInitiated);

        // connection.once('connected', (peerDescriptor: PeerDescriptor,
        // connection: IConnection) => this.onConnected(peerDescriptor,
        // connection)) connection.once('disconnected', (gracefulLeave: boolean)
        // => this.onDisconnected(connection.getPeerDescriptor(),
        // gracefulLeave))
        // return true;
    }

    bool acceptNewConnection(
        const std::shared_ptr<IPendingConnection>& newConnection,
        bool isLocalInitiated) {
        const auto peerDescriptor = newConnection->getPeerDescriptor();
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        // Resolved before taking endpointsMutex: getLocalPeerDescriptor()
        // calls out into the connector facade.
        const auto localNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            this->getLocalPeerDescriptor());
        SLogger::debug("ConnectionManager::acceptNewConnection()");

        // Simultaneous connect: both peers must converge on the SAME
        // physical connection, otherwise one direction's traffic is
        // delivered to a connection the other side is not listening on.
        // The convention is to keep the connection initiated by the
        // offerer. TS decides this with `getOfferer == remote` alone,
        // which is only correct because its single-threaded runtime
        // guarantees a node registers its own outgoing connection before
        // it ever processes the peer's incoming one. Here the connector's
        // dispatcher thread can deliver the incoming connection before the
        // main thread's send() registers the outgoing one; deciding on
        // getOfferer alone would then let the non-offerer replace the
        // offerer's (incoming) connection with its own (outgoing) one, and
        // the two nodes would settle on different connections. So the
        // tie-break is made direction-aware and therefore order-
        // independent: the winning connection is the offerer's — the local
        // node's own outgoing connection when it is the offerer, the
        // remote's incoming connection when it is not.
        const bool localIsOfferer =
            OffererHelper::getOfferer(localNodeId, nodeId) == Offerer::LOCAL;
        const bool newConnectionWins = (isLocalInitiated == localIsOfferer);

        // The exists-check and the insert happen under one lock hold, so
        // two racing accepts for the same peer cannot both insert (the
        // second one would silently orphan the first endpoint). The
        // endpoint calls (setConnecting) run after the lock is released:
        // they take the endpoint's state-machine mutex and lead to
        // call-outs, and holding endpointsMutex across them was one half
        // of the phase-A0 ABBA inversion (the other half being
        // handleDisconnect -> removeSelfFromContainer, which now runs
        // without the state-machine mutex held).
        std::shared_ptr<Endpoint> existingEndpoint;
        std::shared_ptr<Endpoint> createdEndpoint;
        std::uint64_t sequenceNumber = 0;
        {
            SLogger::debug("ConnectionManager::acceptNewConnection() start");
            std::scoped_lock lock(this->endpointsMutex);
            SLogger::debug("Acquired mutex lock in acceptNewConnection");

            const auto it = this->endpoints.find(nodeId);
            if (it != this->endpoints.end()) {
                if (!newConnectionWins) {
                    return false;
                }
                existingEndpoint = it->second;
            } else {
                createdEndpoint = this->createEndpoint(peerDescriptor, nodeId);
                this->endpoints.emplace(nodeId, createdEndpoint);
            }
            // Assigned in decision order under the lock; the endpoint uses
            // it to ignore an adoption that a thread race would otherwise
            // apply out of order.
            sequenceNumber = ++this->connectingSequenceCounter;
        }
        if (existingEndpoint) {
            existingEndpoint->setConnecting(newConnection, sequenceNumber);
            return true;
        }
        createdEndpoint->setConnecting(newConnection, sequenceNumber);
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
        if (endpoint->isConnected()) {
            try {
                SLogger::debug("gracefullyDisconnect() calling blockingWait()");
                streamr::utils::blockingWait(
                    folly::coro::co_invoke(
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
                                     disconnectMode]()
                                        -> folly::coro::Task<void> {
                                        co_return co_await this
                                            ->doGracefullyDisconnectAsync(
                                                targetDescriptor,
                                                disconnectMode);
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
