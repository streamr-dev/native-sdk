// Module streamr.dht.WebrtcConnector
// Ported from packages/dht/src/connection/webrtc/WebrtcConnector.ts
// (v103.8.0-rc.3): opens WebRTC connections by exchanging offer/answer/ICE
// notifications over the signalling transport; the XOR-id tie-break
// (OffererHelper) decides which side offers. Adaptations:
//  - the fire-and-forget TS signalling calls (.catch(trace)) run as
//    detached coroutines on a serial view of the shared worker pool,
//    tracked by a GuardedAsyncScope that stop() drains after abort (the
//    streamr.utils.SharedExecutors architecture); each task builds its own
//    RpcRemote from copied descriptors, so nothing dangles if the caller
//    moves on;
//  - the handshakers TS keeps alive through closures are stored in a map
//    and dropped on HandshakerStopped (the websocket connectors' pattern);
//  - ongoingConnectAttempts is guarded by mMutex (TS has the event loop);
//  - the non-offering branch's manual accept/reject on handshakeRequest is
//    what the C++ IncomingHandshaker already does internally (version
//    check + accept against the pending connection it is given).
module;

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <folly/executors/CPUThreadPoolExecutor.h>

// std::coroutine_traits must be visible in every translation unit
// that defines OR instantiates a coroutine; it cannot arrive through
// an imported BMI.
#include <coroutine> // IWYU pragma: keep

export module streamr.dht.WebrtcConnector;

import streamr.dht.protos;

import streamr.dht.Connection;
import streamr.dht.DhtCallContext;
import streamr.dht.Errors;
import streamr.dht.Handshaker;
import streamr.dht.Identifiers;
import streamr.dht.IncomingHandshaker;
import streamr.dht.IPendingConnection;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.Offerer;
import streamr.dht.OutgoingHandshaker;
import streamr.dht.PendingConnection;
import streamr.dht.PortRange;
import streamr.dht.Transport;
import streamr.dht.WebrtcConnection;
import streamr.dht.WebrtcConnectorRpcLocal;
import streamr.dht.WebrtcConnectorRpcRemote;
import streamr.dht.webrtcTypes;
import streamr.logger.SLogger;
import streamr.protorpc.RpcCommunicator;
import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::protorpc::RpcCommunicatorOptions;
using streamr::utils::AbortController;

export namespace streamr::dht::connection::webrtc {

using namespace std::chrono_literals;
using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::Handshaker;
using streamr::dht::connection::IncomingHandshaker;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::OutgoingHandshaker;
using streamr::dht::connection::PendingConnection;
using streamr::dht::helpers::CannotConnectToSelf;
using streamr::dht::helpers::Offerer;
using streamr::dht::helpers::OffererHelper;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::dht::types::PortRange;

// TS replaceInternalIpWithExternalIp (exported for tests): rewrites the
// connection address of host-type candidates.
inline std::string replaceInternalIpWithExternalIp(
    const std::string& candidate, const std::string& ip) {
    std::vector<std::string> parsed;
    size_t start = 0;
    while (start <= candidate.size()) {
        const auto end = candidate.find(' ', start);
        if (end == std::string::npos) {
            parsed.push_back(candidate.substr(start));
            break;
        }
        parsed.push_back(candidate.substr(start, end - start));
        start = end + 1;
    }
    constexpr size_t addressIndex = 4;
    constexpr size_t typeIndex = 7;
    if (parsed.size() > typeIndex && parsed[typeIndex] == "host") {
        parsed[addressIndex] = ip;
    }
    std::string joined;
    for (const auto& part : parsed) {
        if (!joined.empty()) {
            joined += " ";
        }
        joined += part;
    }
    return joined;
}

struct WebrtcConnectorOptions {
    std::function<bool(const std::shared_ptr<IPendingConnection>&)>
        onNewConnection;
    Transport& transport;
    std::vector<IceServer> iceServers = {};
    std::optional<bool> allowPrivateAddresses = std::nullopt;
    std::optional<size_t> bufferThresholdLow = std::nullopt;
    std::optional<size_t> bufferThresholdHigh = std::nullopt;
    std::optional<size_t> maxMessageSize = std::nullopt;
    std::optional<std::string> externalIp = std::nullopt;
    std::optional<PortRange> portRange = std::nullopt;
};

class WebrtcConnector {
public:
    static constexpr auto webrtcConnectorServiceId = "system/webrtc-connector";

private:
    static constexpr auto rpcRequestTimeout = 15000ms;

    WebrtcConnectorOptions options;
    ListeningRpcCommunicator rpcCommunicator;
    std::map<DhtAddress, ConnectingConnection> ongoingConnectAttempts;
    std::map<DhtAddress, std::shared_ptr<Handshaker>> handshakers;
    std::optional<PeerDescriptor> localPeerDescriptor;
    bool stopped = false;
    std::unique_ptr<WebrtcConnectorRpcLocal> rpcLocal;
    AbortController abortController;
    // Detached signalling notifications (the TS .catch(trace) promises) run
    // on a serial view of the shared worker pool; stop() drains the scope
    // after abort.
    streamr::utils::SharedSerialExecutor signallingExecutor{
        streamr::utils::SharedExecutors::worker()};
    streamr::utils::GuardedAsyncScope signallingScope;
    std::recursive_mutex mMutex;

public:
    explicit WebrtcConnector(WebrtcConnectorOptions&& options)
        : options(std::move(options)),
          rpcCommunicator(
              ServiceID{webrtcConnectorServiceId},
              this->options.transport,
              RpcCommunicatorOptions{.rpcRequestTimeout = rpcRequestTimeout}) {
        this->registerLocalRpcMethods();
    }

    ~WebrtcConnector() { SLogger::trace("~WebrtcConnector()"); }

    std::shared_ptr<IPendingConnection> connect(
        const PeerDescriptor& targetPeerDescriptor,
        bool doNotRequestConnection) {
        std::scoped_lock lock(this->mMutex);
        if (Identifiers::areEqualPeerDescriptors(
                targetPeerDescriptor, this->localPeerDescriptor.value())) {
            throw CannotConnectToSelf("Cannot open WebRTC Connection to self");
        }
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetPeerDescriptor);
        SLogger::trace("Opening WebRTC connection to " + nodeId);
        const auto existingConnection =
            this->ongoingConnectAttempts.find(nodeId);
        if (existingConnection != this->ongoingConnectAttempts.end()) {
            return existingConnection->second.managedConnection;
        }

        auto connection = WebrtcConnection::newInstance(
            WebrtcConnectionParams{
                .remotePeerDescriptor = targetPeerDescriptor,
                .bufferThresholdHigh = this->options.bufferThresholdHigh,
                .bufferThresholdLow = this->options.bufferThresholdLow,
                .iceServers = this->options.iceServers,
                .portRange = this->options.portRange});

        const auto localNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            this->localPeerDescriptor.value());
        const bool offering =
            OffererHelper::getOfferer(localNodeId, nodeId) == Offerer::LOCAL;
        auto pendingConnection =
            PendingConnection::newInstance(targetPeerDescriptor);

        std::shared_ptr<Handshaker> handshaker;
        if (offering) {
            handshaker = OutgoingHandshaker::newInstance(
                this->localPeerDescriptor.value(),
                connection,
                targetPeerDescriptor,
                pendingConnection);
            connection->webrtcEvents()
                .once<webrtcconnectionevents::LocalDescription>(
                    [this, targetPeerDescriptor, connection](
                        const std::string& description,
                        const std::string& /*type*/) {
                        SLogger::trace("Sending offer to remote peer");
                        const auto connectionId = connection->getConnectionID();
                        this->sendSignallingNotification(
                            targetPeerDescriptor,
                            "rtcOffer",
                            [description,
                             connectionId](WebrtcConnectorRpcRemote& remote) {
                                return remote.sendRtcOffer(
                                    description, connectionId);
                            });
                    });
        } else {
            // The incoming handshaker resolves the handshake against this
            // pending connection (and does the version check + accept /
            // reject the TS branch spells out inline).
            auto pendingConnectionCopy = pendingConnection;
            handshaker = IncomingHandshaker::newInstance(
                this->localPeerDescriptor.value(),
                connection,
                [pendingConnectionCopy](const DhtAddress& /*nodeId*/)
                    -> std::shared_ptr<IPendingConnection> {
                    return pendingConnectionCopy;
                });
            connection->webrtcEvents()
                .once<webrtcconnectionevents::LocalDescription>(
                    [this, targetPeerDescriptor, connection](
                        const std::string& description,
                        const std::string& /*type*/) {
                        // Live id: rtcOffer adopted the offerer's connection
                        // id after connect() (TS reads connection.connectionId
                        // at emit time, so the answer carries the offerer's
                        // id and matches on the far side).
                        const auto connectionId = connection->getConnectionID();
                        this->sendSignallingNotification(
                            targetPeerDescriptor,
                            "rtcAnswer",
                            [description,
                             connectionId](WebrtcConnectorRpcRemote& remote) {
                                return remote.sendRtcAnswer(
                                    description, connectionId);
                            });
                    });
        }

        this->ongoingConnectAttempts.emplace(
            nodeId,
            ConnectingConnection{
                .managedConnection = pendingConnection,
                .connection = connection});
        this->handshakers.emplace(nodeId, handshaker);
        handshaker->on<handshakerevents::HandshakerStopped>([this, nodeId]() {
            std::scoped_lock lock(this->mMutex);
            this->handshakers.erase(nodeId);
        });

        // TS delFunc: the attempt is dropped once the connection settles
        // either way. Every handler below erases the map entry ONLY if it
        // still belongs to the connection/pending pair that fired — the map
        // is keyed by nodeId and repeated connects to the same peer insert
        // new pairs, so an unconditional erase(nodeId) from a stale handler
        // (an older connection's Disconnected arriving late) deleted a
        // freshly inserted attempt and orphaned its WebrtcConnection
        // (observed: insert and stale erase within the same millisecond).
        // An orphan is immortal — its rtc callbacks capture a strong self,
        // broken only by doClose — and its dispatch executor's worker-pool
        // KeepAlive then blocks process exit in the shared pool destructor
        // (joinKeepAliveOnce). The captured raw pointers are compared for
        // identity only, never dereferenced.
        connection->on<connectionevents::Disconnected>(
            [this, nodeId, connPtr = connection.get()](
                bool /*gracefulLeave*/,
                uint64_t /*code*/,
                const std::string& /*reason*/) {
                std::scoped_lock lock(this->mMutex);
                const auto it = this->ongoingConnectAttempts.find(nodeId);
                if (it == this->ongoingConnectAttempts.end()) {
                    return;
                }
                if (it->second.connection.get() != connPtr) {
                    return;
                }
                this->ongoingConnectAttempts.erase(it);
            });
        pendingConnection->on<pendingconnectionevents::Disconnected>(
            [this, nodeId, pendingPtr = pendingConnection.get()](
                bool /*gracefulLeave*/) {
                // The attempt's WebrtcConnection must be closed HERE, not
                // just dropped from the map: for an answerer that never
                // received a handshake request, no handshaker links the
                // pending connection's Disconnected to connection->close()
                // (IncomingHandshaker wires that link only inside
                // onHandshakeRequest), so a bare erase would remove the last
                // owner stop() could still destroy and leave the connection
                // orphaned (see the immortality note above — this was the
                // 1-in-10 post-PASSED exit hang of the 22-node webrtc test).
                // close() is idempotent, so the offerer path (whose
                // OutgoingHandshaker closes the connection from this same
                // event first) is unaffected. Identity-checked like the
                // handler above.
                std::shared_ptr<WebrtcConnection> connectionToClose;
                {
                    std::scoped_lock lock(this->mMutex);
                    const auto it = this->ongoingConnectAttempts.find(nodeId);
                    if (it == this->ongoingConnectAttempts.end() ||
                        it->second.managedConnection.get() != pendingPtr) {
                        return;
                    }
                    connectionToClose = it->second.connection;
                    this->ongoingConnectAttempts.erase(it);
                }
                // Outside mMutex (locking policy: no call-outs under the
                // connector mutex — the close emits Disconnected and defers
                // the rtc teardown to the worker pool).
                if (connectionToClose) {
                    connectionToClose->close(false);
                }
            });
        pendingConnection->on<pendingconnectionevents::Connected>(
            [this, nodeId, pendingPtr = pendingConnection.get()](
                const PeerDescriptor& /*peerDescriptor*/,
                const std::shared_ptr<Connection>& /*connection*/) {
                std::scoped_lock lock(this->mMutex);
                const auto it = this->ongoingConnectAttempts.find(nodeId);
                if (it == this->ongoingConnectAttempts.end() ||
                    it->second.managedConnection.get() != pendingPtr) {
                    return;
                }
                this->ongoingConnectAttempts.erase(it);
            });

        connection->webrtcEvents().on<webrtcconnectionevents::LocalCandidate>(
            [this, targetPeerDescriptor, connection](
                const std::string& candidate, const std::string& mid) {
                const auto connectionId = connection->getConnectionID();
                std::string effectiveCandidate = candidate;
                if (this->options.externalIp.has_value()) {
                    effectiveCandidate = replaceInternalIpWithExternalIp(
                        candidate, *this->options.externalIp);
                    SLogger::debug(
                        "onLocalCandidate injected external ip " +
                        effectiveCandidate + " " + mid);
                }
                this->sendSignallingNotification(
                    targetPeerDescriptor,
                    "iceCandidate",
                    [effectiveCandidate, mid, connectionId](
                        WebrtcConnectorRpcRemote& remote) {
                        return remote.sendIceCandidate(
                            effectiveCandidate, mid, connectionId);
                    });
            });

        connection->start(offering);

        if (!doNotRequestConnection && !offering) {
            this->sendSignallingNotification(
                targetPeerDescriptor,
                "requestConnection",
                [](WebrtcConnectorRpcRemote& remote) {
                    return remote.requestConnection();
                });
        }

        return pendingConnection;
    }

    void setLocalPeerDescriptor(const PeerDescriptor& peerDescriptor) {
        std::scoped_lock lock(this->mMutex);
        this->localPeerDescriptor = peerDescriptor;
    }

    void stop() {
        SLogger::trace("stop()");
        std::map<DhtAddress, ConnectingConnection> attempts;
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped) {
                return;
            }
            this->stopped = true;
            this->abortController.abort();
            attempts = this->ongoingConnectAttempts;
            this->ongoingConnectAttempts.clear();
        }
        // Outside the mutex: destroying connections emits events whose
        // handlers take it, and the scope drain must not hold it either.
        for (auto& [nodeId, attempt] : attempts) {
            attempt.connection->destroy();
            attempt.managedConnection->close(false);
        }
        this->signallingScope.close();
        this->rpcCommunicator.destroy();
    }

private:
    void registerLocalRpcMethods() {
        this->rpcLocal = std::make_unique<WebrtcConnectorRpcLocal>(
            WebrtcConnectorRpcLocalOptions{
                .connect = [this](
                               const PeerDescriptor& targetPeerDescriptor,
                               bool doNotRequestConnection)
                    -> std::shared_ptr<IPendingConnection> {
                    return this->connect(
                        targetPeerDescriptor, doNotRequestConnection);
                },
                .onNewConnection =
                    [this](
                        const std::shared_ptr<IPendingConnection>& connection) {
                        return this->options.onNewConnection(connection);
                    },
                .getOngoingConnectAttempt = [this](const DhtAddress& nodeId)
                    -> std::optional<ConnectingConnection> {
                    std::scoped_lock lock(this->mMutex);
                    const auto it = this->ongoingConnectAttempts.find(nodeId);
                    if (it == this->ongoingConnectAttempts.end()) {
                        return std::nullopt;
                    }
                    return it->second;
                },
                .getLocalPeerDescriptor =
                    [this]() {
                        std::scoped_lock lock(this->mMutex);
                        return this->localPeerDescriptor.value();
                    },
                .allowPrivateAddresses =
                    this->options.allowPrivateAddresses.value_or(true)});

        this->rpcCommunicator
            .registerRpcNotification<::dht::WebrtcConnectionRequest>(
                "requestConnection",
                [this](
                    const ::dht::WebrtcConnectionRequest& request,
                    const DhtCallContext& context) {
                    std::scoped_lock lock(this->mMutex);
                    if (this->stopped) {
                        return;
                    }
                    this->rpcLocal->requestConnection(request, context);
                });
        this->rpcCommunicator.registerRpcNotification<::dht::RtcOffer>(
            "rtcOffer",
            [this](
                const ::dht::RtcOffer& request, const DhtCallContext& context) {
                std::scoped_lock lock(this->mMutex);
                if (this->stopped) {
                    return;
                }
                this->rpcLocal->rtcOffer(request, context);
            });
        this->rpcCommunicator.registerRpcNotification<::dht::RtcAnswer>(
            "rtcAnswer",
            [this](
                const ::dht::RtcAnswer& request,
                const DhtCallContext& context) {
                std::scoped_lock lock(this->mMutex);
                if (this->stopped) {
                    return;
                }
                this->rpcLocal->rtcAnswer(request, context);
            });
        this->rpcCommunicator.registerRpcNotification<::dht::IceCandidate>(
            "iceCandidate",
            [this](
                const ::dht::IceCandidate& request,
                const DhtCallContext& context) {
                std::scoped_lock lock(this->mMutex);
                if (this->stopped) {
                    return;
                }
                this->rpcLocal->iceCandidate(request, context);
            });
    }

    // Fires one signalling notification detached (the TS
    // remoteConnector.sendX().catch() shape): the task owns copies of both
    // descriptors and builds its own client/remote, is cancellable by
    // stop()'s abort, and is drained by the scope before the communicator
    // is destroyed.
    void sendSignallingNotification(
        const PeerDescriptor& targetPeerDescriptor,
        std::string description,
        std::function<folly::coro::Task<void>(WebrtcConnectorRpcRemote&)>
            makeCall) {
        PeerDescriptor local;
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped || !this->localPeerDescriptor.has_value()) {
                return;
            }
            local = this->localPeerDescriptor.value();
        }
        this->signallingScope.add(
            streamr::utils::co_withExecutor(
                &this->signallingExecutor,
                folly::coro::co_invoke(
                    [this,
                     local,
                     target = targetPeerDescriptor,
                     description = std::move(description),
                     makeCall =
                         std::move(makeCall)]() -> folly::coro::Task<void> {
                        try {
                            WebrtcConnectorRpcClient client(
                                this->rpcCommunicator);
                            WebrtcConnectorRpcRemote remote(
                                PeerDescriptor(local),
                                PeerDescriptor(target),
                                std::move(client));
                            co_await streamr::utils::co_withCancellation(
                                this->abortController.getSignal()
                                    .getCancellationToken(),
                                makeCall(remote));
                        } catch (const std::exception& err) {
                            SLogger::trace(
                                "Failed to send " + description + " " +
                                std::string(err.what()));
                        }
                    })));
    }
};

} // namespace streamr::dht::connection::webrtc
