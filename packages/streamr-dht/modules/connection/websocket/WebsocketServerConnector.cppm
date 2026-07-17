// Module streamr.dht.WebsocketServerConnector
// CONSOLIDATED from the former header
// streamr-dht/connection/websocket/WebsocketServerConnector.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;
#include <algorithm>
#include <chrono>
// std::coroutine_traits must be visible in every translation unit
// that defines OR instantiates a coroutine; it cannot arrive through
// an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <functional>
#include <map>
#include <random>
#include <thread>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <mutex>

#include <folly/executors/CPUThreadPoolExecutor.h>

export module streamr.dht.WebsocketServerConnector;

import streamr.dht.protos;

import streamr.dht.WebsocketServerConnection;
import streamr.dht.Connection;
import streamr.dht.Connectivity;
import streamr.dht.WebsocketClientConnectorRpcRemote;
import streamr.dht.connectivityChecker;
import streamr.dht.connectivityRequestHandler;
import streamr.dht.Errors;
import streamr.logger.SLogger;
import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;
import streamr.utils.Ipv4Helper;
import streamr.utils.Uuid;
import streamr.dht.Handshaker;
import streamr.dht.IPendingConnection;
import streamr.dht.Identifiers;
import streamr.dht.IncomingHandshaker;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.NatType;
import streamr.dht.PendingConnection;
import streamr.dht.PortRange;
import streamr.dht.TlsCertificateFiles;
import streamr.dht.Transport;
import streamr.dht.Version;
import streamr.dht.WebsocketServer;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
using streamr::utils::AbortController;
using streamr::utils::Ipv4Helper;

using streamr::utils::Uuid;

export namespace streamr::dht::connection::websocket {

using ::dht::ConnectivityRequest;
using ::dht::ConnectivityResponse;
using streamr::dht::connection::IncomingHandshaker;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::websocket::WebsocketClientConnectorRpcClient;
using streamr::dht::connection::websocket::WebsocketClientConnectorRpcRemote;
using streamr::dht::connection::websocket::WebsocketServer;
using streamr::dht::connection::websocket::WebsocketServerConnection;
using streamr::dht::helpers::Connectivity;
using streamr::dht::helpers::Version;
using streamr::dht::helpers::WebsocketServerStartError;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::dht::types::PortRange;
using streamr::dht::types::TlsCertificateFiles;

// NOLINTNEXTLINE(readability-identifier-naming)
namespace NatType = streamr::dht::types::NatType;

struct WebsocketServerConnectorOptions {
    std::function<bool(const std::shared_ptr<IPendingConnection>&)>
        onNewConnection;
    ListeningRpcCommunicator& rpcCommunicator;
    std::function<bool(DhtAddress)> hasConnection;
    std::optional<PortRange> portRange = std::nullopt;
    std::optional<size_t> maxMessageSize = std::nullopt;
    std::optional<std::string> host = std::nullopt;
    std::optional<std::vector<PeerDescriptor>> entrypoints = std::nullopt;
    std::optional<TlsCertificateFiles> tlsCertificateFiles = std::nullopt;
    std::optional<Transport*> autoCertifierTransport = std::nullopt;
    std::optional<std::string> autoCertifierUrl = std::nullopt;
    std::optional<std::string> autoCertifierConfigFile = std::nullopt;
    std::optional<bool> serverEnableTls = std::nullopt;
    std::optional<std::string> geoIpDatabaseFolder = std::nullopt;
};

inline constexpr std::chrono::milliseconds entrypointRetryDelay{2000};
inline constexpr std::chrono::milliseconds abortablePollInterval{100};

class WebsocketServerConnector {
private:
    WebsocketServerConnectorOptions options;
    std::unique_ptr<WebsocketServer> websocketServer;
    std::optional<std::string> host;
    std::optional<PeerDescriptor> localPeerDescriptor;
    AbortController abortController;
    std::optional<uint16_t> selectedPort;
    std::map<std::string, std::shared_ptr<IncomingHandshaker>>
        connectingHandshakers;
    std::map<DhtAddress, std::shared_ptr<IPendingConnection>>
        ongoingConnectRequests;
    // Runs the detached requestConnection notifications (TS setImmediate)
    // on a serial view of the shared worker pool (per-instance pools are
    // forbidden — see streamr.utils.SharedExecutors); destroy() drains the
    // scope after abort, and the gate drops a connect() racing the drain.
    streamr::utils::SharedSerialExecutor requestConnectionExecutor{
        streamr::utils::SharedExecutors::worker()};
    streamr::utils::GuardedAsyncScope requestConnectionScope;
    std::recursive_mutex mMutex;

public:
    // Members initialize in DECLARATION order (options first), so `host`
    // must read from the already-moved-into MEMBER, not the parameter — the
    // old `host(options.host)` read the moved-from parameter and produced an
    // empty host, which surfaced as a `ws://:port` connectivity URL once the
    // end-to-end path used the advertised descriptor.
    explicit WebsocketServerConnector(WebsocketServerConnectorOptions&& options)
        : options(std::move(options)), host(this->options.host) {
        if (this->options.portRange.has_value()) {
            this->websocketServer = std::make_unique<WebsocketServer>(std::move(
                WebsocketServerConfig{
                    .portRange = this->options.portRange.value(),
                    .enableTls = this->options.serverEnableTls.value_or(false),
                    .tlsCertificateFiles = this->options.tlsCertificateFiles,
                    .maxMessageSize = this->options.maxMessageSize}));
        }
    }
    ~WebsocketServerConnector() {
        SLogger::info("WebsocketServerConnector::~WebsocketServerConnector");
    }

    static std::string getActionFromUrl(const Url& resourceUrl) {
        std::string action;
        size_t queryPos = resourceUrl.find("?");
        if (queryPos != std::string::npos) {
            std::string query = resourceUrl.substr(queryPos + 1);
            size_t actionPos = query.find("action=");
            if (actionPos != std::string::npos) {
                size_t valueStart = actionPos + 7; // NOLINT length of "action="
                size_t valueEnd = query.find('&', valueStart);
                if (valueEnd == std::string::npos) {
                    valueEnd = query.length();
                }
                action = query.substr(valueStart, valueEnd - valueStart);
            }
        }
        return action;
    }

    void start() {
        if (!this->abortController.getSignal().aborted &&
            this->websocketServer) {
            this->websocketServer->on<
                websocketserverevents::
                    Connected>([this](
                                   const std::shared_ptr<
                                       WebsocketServerConnection>&
                                       serverSocket) {
                if (this->abortController.getSignal().aborted) {
                    // destroy() has run (or is running): drop the late
                    // connection instead of touching the cleared maps.
                    try {
                        serverSocket->close(false);
                    } catch (...) {
                        SLogger::debug(
                            "Error closing incoming Websocket connection"
                            " after abort");
                    }
                    return;
                }
                const auto resourceUrl = serverSocket->getResourceURL();
                const auto action = getActionFromUrl(resourceUrl);
                SLogger::trace(
                    "WebSocket client connected",
                    {{"action", action},
                     {"remoteAddress", serverSocket->getRemoteAddress()}});

                if (action == "connectivityRequest") {
                    SLogger::trace("Connectivity request received");
                    // GeoIP location lookup omitted (deferred to milestone
                    // E with the rest of GeoIP).
                    attachConnectivityRequestHandler(serverSocket);
                } else if (action == "connectivityProbe") {
                    // The probe only needs the websocket connection to
                    // open; the prober closes it (TS: no-op). The listener
                    // pins the socket until then (same idiom as
                    // attachConnectivityRequestHandler: onClosed drops all
                    // listeners, releasing it).
                    SLogger::trace("Connectivity probe received");
                    serverSocket->on<connectionevents::Disconnected>(
                        [serverSocket](
                            bool /*gracefulLeave*/,
                            uint64_t /*code*/,
                            const std::string& /*reason*/) {});
                } else {
                    // The localPeerDescriptor can be undefined here as the
                    // WS server is used for connectivity checks before the
                    // localPeerDescriptor is set during start. Handshaked
                    // connections should be rejected before the
                    // localPeerDescriptor is set.

                    if (this->localPeerDescriptor.has_value()) {
                        SLogger::trace("Server attaching handshaker");
                        this->attachHandshaker(serverSocket);
                    } else {
                        SLogger::trace(
                            "Incoming Websocket connection before localPeerDescriptor was set, closing connection");
                        try {
                            serverSocket->close(false);
                        } catch (...) {
                            SLogger::error(
                                "Error closing incoming Websocket connection before localPeerDescriptor was set");
                        }
                    }
                }
            });
            this->selectedPort = this->websocketServer->start();
        }
    }

    // TS: wait(2000, abortSignal) between entry-point attempts.
    void waitAbortable(std::chrono::milliseconds duration) {
        const auto deadline = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < deadline &&
               !this->abortController.getSignal().aborted) {
            std::this_thread::sleep_for(abortablePollInterval);
        }
    }

    // The state is snapshotted under the lock and the network runs without
    // it: a connectivity round trip blocks for seconds and the connector
    // must stay operable meanwhile (TS holds no lock either).
    ConnectivityResponse checkConnectivity(bool allowSelfSignedCertificate) {
        std::optional<std::string> hostSnapshot;
        std::optional<uint16_t> portSnapshot;
        bool serverStarted = false;
        std::vector<PeerDescriptor> entrypoints;
        {
            std::scoped_lock lock(this->mMutex);
            hostSnapshot = this->host;
            portSnapshot = this->selectedPort;
            serverStarted = (this->websocketServer != nullptr);
            if (this->options.entrypoints.has_value()) {
                entrypoints = this->options.entrypoints.value();
            }
        }

        ConnectivityResponse response;
        if (this->abortController.getSignal().aborted) {
            response.set_host("127.0.0.1");
            response.set_nattype(NatType::UNKNOWN);
            response.set_ipaddress(Ipv4Helper::ipv4ToNumber("127.0.0.1"));
            response.set_protocolversion(Version::localProtocolVersion);
            return response;
        }
        if (entrypoints.empty()) {
            // Return connectivity info given in options. A connector with
            // no websocket server and no host reports empty host/port here
            // (TS returns undefined fields); callers in that configuration
            // provide their own peer descriptor and ignore the response.
            response.set_host(hostSnapshot.value_or(""));
            response.set_nattype(NatType::OPEN_INTERNET);
            // TODO: Resolve the given host name or use as is if IP was
            // given (mirrors the TS TODO).
            response.set_ipaddress(Ipv4Helper::ipv4ToNumber("127.0.0.1"));
            response.set_protocolversion(Version::localProtocolVersion);
            response.mutable_websocket()->set_host(hostSnapshot.value_or(""));
            response.mutable_websocket()->set_port(portSnapshot.value_or(0));
            response.mutable_websocket()->set_tls(
                this->options.tlsCertificateFiles.has_value());
            return response;
        }
        // Do real connectivity checking against the entry points, in random
        // order, until one answers.
        std::shuffle(
            entrypoints.begin(),
            entrypoints.end(),
            std::mt19937{std::random_device{}()});
        for (size_t i = 0; i < entrypoints.size() &&
             !this->abortController.getSignal().aborted;
             i++) {
            const auto& entryPoint = entrypoints[i];
            try {
                ConnectivityRequest connectivityRequest;
                connectivityRequest.set_port(
                    portSnapshot.value_or(DISABLE_CONNECTIVITY_PROBE));
                if (hostSnapshot.has_value()) {
                    connectivityRequest.set_host(hostSnapshot.value());
                }
                connectivityRequest.set_tls(
                    serverStarted &&
                    this->options.serverEnableTls.value_or(false));
                connectivityRequest.set_allowselfsignedcertificate(
                    allowSelfSignedCertificate);
                return sendConnectivityRequest(connectivityRequest, entryPoint);
            } catch (const std::exception& err) {
                SLogger::error(
                    "Failed to connect to entrypoint with id " +
                    Identifiers::getNodeIdFromPeerDescriptor(entryPoint) + " " +
                    std::string(err.what()));
                this->waitAbortable(entrypointRetryDelay);
            }
        }
        std::string attemptedHosts;
        for (const auto& entryPoint : entrypoints) {
            if (!attemptedHosts.empty()) {
                attemptedHosts += ", ";
            }
            attemptedHosts += entryPoint.websocket().host() + ":" +
                std::to_string(entryPoint.websocket().port());
        }
        throw WebsocketServerStartError(
            "Failed to connect to the entrypoints after " +
            std::to_string(entrypoints.size()) +
            " attempts\nAttempted hosts: " + attemptedHosts);
    }

    bool isPossibleToFormConnection(
        const PeerDescriptor& targetPeerDescriptor) {
        std::scoped_lock lock(this->mMutex);
        const auto connectionType = Connectivity::expectedConnectionType(
            this->localPeerDescriptor.value(), targetPeerDescriptor);
        return connectionType == ConnectionType::WEBSOCKET_SERVER;
    }

    // Ask the target (which has no websocket server) to open a websocket
    // back to this node; the returned pending connection completes when the
    // incoming handshake matches ongoingConnectRequests (attachHandshaker).
    std::shared_ptr<IPendingConnection> connect(
        const PeerDescriptor& targetPeerDescriptor) {
        std::scoped_lock lock(this->mMutex);
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetPeerDescriptor);
        if (this->ongoingConnectRequests.contains(nodeId)) {
            return this->ongoingConnectRequests.at(nodeId);
        }
        return this->requestConnectionFromPeer(
            this->localPeerDescriptor.value(), targetPeerDescriptor);
    }

    void setLocalPeerDescriptor(const PeerDescriptor& localPeerDescriptor) {
        std::scoped_lock lock(this->mMutex);
        this->localPeerDescriptor = localPeerDescriptor;
    }

    void destroy() {
        SLogger::trace("WebsocketServerConnector::destroy() called");
        std::map<std::string, std::shared_ptr<IncomingHandshaker>> handshakers;
        std::map<DhtAddress, std::shared_ptr<IPendingConnection>> requests;
        std::unique_ptr<WebsocketServer> server;
        {
            std::scoped_lock lock(this->mMutex);
            this->abortController.abort();
            handshakers.swap(this->connectingHandshakers);
            requests.swap(this->ongoingConnectRequests);
            server = std::move(this->websocketServer);
        }
        // Every call-out below runs OUTSIDE mMutex: close(true) fans out
        // into Disconnected handlers that re-enter connection/connector
        // code, and holding the lock across that deadlocks ABBA against a
        // thread that holds a connection's mutex and needs mMutex (the
        // shape sampled in WebsocketClientConnector::destroy(); same
        // hazard here).
        SLogger::trace("Closing ongoing connect requests");
        for (const auto& request : requests) {
            if (request.second) {
                request.second->close(true);
            }
        }

        SLogger::trace("Closing connecting handshakers");
        for (const auto& handshaker : handshakers) {
            if (handshaker.second &&
                handshaker.second->getPendingConnection()) {
                handshaker.second->getPendingConnection()->close(true);
            }
        }

        SLogger::trace("Stopping websocket server");
        if (server) {
            server->stop();
            server.reset();
        }
        // Drained OUTSIDE the mutex (the PeerManager::stop() pattern): a
        // detached requestConnection notification still in flight references
        // this object and the RPC communicator; abort above cancels it, the
        // drain makes destroy() wait for it without deadlocking on mMutex.
        this->requestConnectionScope.close();

        SLogger::trace("WebsocketServerConnector::destroy() completed");
    }

private:
    // TS requestConnectionFromPeer. The caller holds mMutex. TS defers the
    // notification with setImmediate; here it runs on a dedicated worker
    // executor so connect() never blocks on RPC I/O.
    std::shared_ptr<IPendingConnection> requestConnectionFromPeer(
        const PeerDescriptor& localPeerDescriptor,
        const PeerDescriptor& targetPeerDescriptor) {
        auto pendingConnection =
            PendingConnection::newInstance(targetPeerDescriptor);
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetPeerDescriptor);
        // TS delFunc: drop the ongoing request once it settles either way
        // (erasing an already-erased key is a no-op).
        pendingConnection->on<pendingconnectionevents::Connected>(
            [this, nodeId](
                const PeerDescriptor& /*peerDescriptor*/,
                const std::shared_ptr<Connection>& /*connection*/) {
                std::scoped_lock lock(this->mMutex);
                this->ongoingConnectRequests.erase(nodeId);
            });
        pendingConnection->on<pendingconnectionevents::Disconnected>(
            [this, nodeId](bool /*gracefulLeave*/) {
                std::scoped_lock lock(this->mMutex);
                this->ongoingConnectRequests.erase(nodeId);
            });
        this->ongoingConnectRequests.emplace(nodeId, pendingConnection);
        this->requestConnectionScope.add(
            streamr::utils::co_withExecutor(
                &this->requestConnectionExecutor,
                folly::coro::co_invoke(
                    [this, localPeerDescriptor, targetPeerDescriptor]()
                        -> folly::coro::Task<void> {
                        try {
                            WebsocketClientConnectorRpcClient client(
                                this->options.rpcCommunicator);
                            WebsocketClientConnectorRpcRemote remoteConnector(
                                PeerDescriptor(localPeerDescriptor),
                                PeerDescriptor(targetPeerDescriptor),
                                std::move(client));
                            // Cancellable by destroy(): abort fires before the
                            // executor join drains this task.
                            co_await streamr::utils::co_withCancellation(
                                this->abortController.getSignal()
                                    .getCancellationToken(),
                                remoteConnector.requestConnection());
                            SLogger::trace(
                                "Sent WebsocketConnectionRequest notification"
                                " to peer");
                        } catch (const std::exception& err) {
                            SLogger::debug(
                                "Failed to send WebsocketConnectionRequest"
                                " notification to peer " +
                                std::string(err.what()));
                        }
                    })));
        return pendingConnection;
    }

    void attachHandshaker(
        const std::shared_ptr<WebsocketServerConnection>& serverSocket) {
        std::scoped_lock lock(this->mMutex);
        const auto handshakerId = Uuid::v4();

        auto handshaker = IncomingHandshaker::newInstance(
            this->localPeerDescriptor.value(),
            serverSocket,
            [this, handshakerId](const DhtAddress& nodeId)
                -> std::shared_ptr<IPendingConnection> {
                std::scoped_lock lock(this->mMutex);

                if (this->ongoingConnectRequests.contains(nodeId)) {
                    return this->ongoingConnectRequests.at(nodeId);
                }

                return nullptr;
            },
            this->options.onNewConnection);

        this->connectingHandshakers.emplace(
            handshakerId, std::move(handshaker));

        this->connectingHandshakers.at(handshakerId)
            ->on<handshakerevents::HandshakerStopped>([this, handshakerId]() {
                std::scoped_lock lock(this->mMutex);
                if (this->connectingHandshakers.contains(handshakerId)) {
                    this->connectingHandshakers.erase(handshakerId);
                }
            });
    }
};

} // namespace streamr::dht::connection::websocket
