// Module streamr.trackerlessnetwork.Inspector
// Ported from packages/trackerless-network/src/content-delivery-layer/
// inspection/Inspector.ts (v103.8.0-rc.3): opens a temporary connection to
// a suspected-misbehaving node, runs an InspectSession over the stream
// part's traffic, and reports whether the node forwards messages. The
// open/close operations are overridable callbacks (TS options style) so
// the unit test can substitute fakes.
module;

#include <chrono>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <coroutine> // IWYU pragma: keep

export module streamr.trackerlessnetwork.Inspector;

import streamr.dht.protos;
import streamr.trackerlessnetwork.protos;

import streamr.dht.ConnectionLocker;
// export import: LockID appears in this module's public callback
// signatures, so every consumer needs the declaration visible.
export import streamr.dht.ConnectionLockStates;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.logger.SLogger;
import streamr.trackerlessnetwork.InspectSession;
import streamr.trackerlessnetwork.TemporaryConnectionRpcRemote;
import streamr.utils.CoroutineHelper;
import streamr.utils.StreamPartID;
import streamr.utils.waitForEvent;

// Hoisted from the former-header idiom (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::trackerlessnetwork::inspection {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::LockID;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::trackerlessnetwork::TemporaryConnectionRpcClient;
using streamr::trackerlessnetwork::TemporaryConnectionRpcRemote;
using streamr::utils::StreamPartID;

using InspectConnectionFn =
    std::function<folly::coro::Task<void>(PeerDescriptor, LockID)>;

struct InspectorOptions {
    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    ListeningRpcCommunicator& rpcCommunicator;
    ConnectionLocker& connectionLocker;
    std::optional<std::chrono::milliseconds> inspectionTimeout;
    // Empty function = use the default temporary-connection RPC.
    InspectConnectionFn openInspectConnection;
    InspectConnectionFn closeInspectConnection;
};

class Inspector {
private:
    static constexpr std::chrono::milliseconds defaultInspectionTimeout{
        60 * 1000};

    PeerDescriptor localPeerDescriptor;
    StreamPartID streamPartId;
    ListeningRpcCommunicator& rpcCommunicator;
    ConnectionLocker& connectionLocker;
    std::chrono::milliseconds inspectionTimeout;
    InspectConnectionFn openInspectConnection;
    InspectConnectionFn closeInspectConnection;
    // TS runs single-threaded; here markMessage() arrives from delivery
    // threads while inspect() mutates the map on the caller's thread.
    std::mutex mutex;
    std::map<DhtAddress, std::shared_ptr<InspectSession>> sessions;

    folly::coro::Task<void> defaultOpenInspectConnection(
        PeerDescriptor peerDescriptor, LockID lockId) {
        TemporaryConnectionRpcRemote rpcRemote(
            this->localPeerDescriptor,
            peerDescriptor,
            TemporaryConnectionRpcClient(this->rpcCommunicator));
        co_await rpcRemote.openConnection();
        this->connectionLocker.weakLockConnection(
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor), lockId);
    }

    folly::coro::Task<void> defaultCloseInspectConnection(
        PeerDescriptor peerDescriptor, LockID lockId) {
        TemporaryConnectionRpcRemote rpcRemote(
            this->localPeerDescriptor,
            peerDescriptor,
            TemporaryConnectionRpcClient(this->rpcCommunicator));
        co_await rpcRemote.closeConnection();
        this->connectionLocker.weakUnlockConnection(
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor), lockId);
    }

public:
    explicit Inspector(InspectorOptions options)
        : localPeerDescriptor(std::move(options.localPeerDescriptor)),
          streamPartId(std::move(options.streamPartId)),
          rpcCommunicator(options.rpcCommunicator),
          connectionLocker(options.connectionLocker),
          inspectionTimeout(
              options.inspectionTimeout.value_or(defaultInspectionTimeout)),
          openInspectConnection(
              options.openInspectConnection
                  ? std::move(options.openInspectConnection)
                  : [this](
                        PeerDescriptor peerDescriptor,
                        LockID lockId) -> folly::coro::Task<void> {
                  co_return co_await this->defaultOpenInspectConnection(
                      std::move(peerDescriptor), std::move(lockId));
              }),
          closeInspectConnection(
              options.closeInspectConnection
                  ? std::move(options.closeInspectConnection)
                  : [this](
                        PeerDescriptor peerDescriptor,
                        LockID lockId) -> folly::coro::Task<void> {
                  co_return co_await this->defaultCloseInspectConnection(
                      std::move(peerDescriptor), std::move(lockId));
              }) {}

    folly::coro::Task<bool> inspect(PeerDescriptor peerDescriptor) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        auto session = std::make_shared<InspectSession>(
            InspectSessionOptions{.inspectedNode = nodeId});
        const LockID lockId{"inspector-" + this->streamPartId};
        {
            std::scoped_lock lock(this->mutex);
            this->sessions[nodeId] = session;
        }
        co_await this->openInspectConnection(peerDescriptor, lockId);
        bool success = false;
        try {
            co_await streamr::utils::waitForEvent<inspectsessionevents::Done>(
                session.get(), this->inspectionTimeout);
            success = true;
        } catch (const std::exception&) {
            SLogger::trace("Inspect session timed out, removing");
        }
        // TS finally block:
        co_await this->closeInspectConnection(peerDescriptor, lockId);
        {
            std::scoped_lock lock(this->mutex);
            this->sessions.erase(nodeId);
        }
        co_return success || session->getInspectedMessageCount() < 1 ||
            session->onlyMarkedByInspectedNode();
    }

    void markMessage(const DhtAddress& sender, const ::MessageID& messageId) {
        // Sessions are marked outside the map lock: markMessage may emit
        // Done into listeners that take their own locks.
        std::vector<std::shared_ptr<InspectSession>> currentSessions;
        {
            std::scoped_lock lock(this->mutex);
            currentSessions.reserve(this->sessions.size());
            for (const auto& [_, session] : this->sessions) {
                currentSessions.push_back(session);
            }
        }
        for (const auto& session : currentSessions) {
            session->markMessage(sender, messageId);
        }
    }

    [[nodiscard]] bool isInspected(const DhtAddress& nodeId) {
        std::scoped_lock lock(this->mutex);
        return this->sessions.contains(nodeId);
    }

    void stop() {
        std::vector<std::shared_ptr<InspectSession>> currentSessions;
        {
            std::scoped_lock lock(this->mutex);
            for (const auto& [_, session] : this->sessions) {
                currentSessions.push_back(session);
            }
            this->sessions.clear();
        }
        for (const auto& session : currentSessions) {
            session->stop();
        }
    }
};

} // namespace streamr::trackerlessnetwork::inspection
