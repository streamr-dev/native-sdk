// Module streamr.dht.RecursiveOperationManager
// Ported from packages/dht/src/dht/recursive-operation/
// RecursiveOperationManager.ts (v103.8.0-rc.3). Drives recursive DHT
// operations (find-closest-nodes, fetch-data, delete-data): it forwards a
// routeRequest towards the target over the A4 Router (RECURSIVE mode),
// answers with the closest connected nodes and any locally stored data,
// and runs an initiator-side RecursiveOperationSession to collect results.
//
// The three Router operations are taken as callbacks (doRouteMessage /
// isMostLikelyDuplicate / addToDuplicateDetector) rather than a Router
// handle, matching this package's callback-options style (PeerManager,
// DhtNodeRpcLocal, RouterRpcLocal) and letting the unit test substitute a
// mock router.
module;

#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.RecursiveOperationManager;

import streamr.utils.CoroutineHelper;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.waitForEvent;
import streamr.logger.SLogger;
import streamr.protorpc.RpcCommunicator;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.DhtRpcClient;
import streamr.dht.getPeerDistance;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.LocalDataStore;
import streamr.dht.RecursiveOperationRpcLocal;
import streamr.dht.RecursiveOperationSession;
import streamr.dht.RecursiveOperationSessionRpcRemote;
import streamr.dht.RouterRpcLocal;
import streamr.dht.RoutingSession;
import streamr.dht.SortedContactList;
import streamr.dht.Transport;
import streamr.dht.ConnectionsView;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;

export namespace streamr::dht::recursiveoperation {

using ::dht::DataEntry;
using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperation;
using ::dht::RecursiveOperationRequest;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageError;
using ::dht::RouteMessageWrapper;
using streamr::dht::DhtAddress;
using streamr::dht::DhtNodeRpcRemote;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::contact::SortedContactList;
using streamr::dht::contact::SortedContactListOptions;
using streamr::dht::helpers::getPeerDistance;
using streamr::dht::routing::createRouteMessageAck;
using streamr::dht::routing::RoutingMode;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::store::LocalDataStore;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::RpcCommunicatorOptions;

using RecursiveOperationSessionRpcClient =
    ::dht::RecursiveOperationSessionRpcClient<DhtCallContext>;

struct RecursiveOperationManagerOptions {
    RpcCommunicator<DhtCallContext>& rpcCommunicator;
    Transport& sessionTransport;
    ConnectionsView& connectionsView;
    PeerDescriptor localPeerDescriptor;
    ServiceID serviceId;
    LocalDataStore& localDataStore;
    std::function<void(const PeerDescriptor&)> addContact;
    std::function<std::shared_ptr<DhtNodeRpcRemote>(const PeerDescriptor&)>
        createDhtNodeRpcRemote;
    // The Router operations (see the module comment).
    std::function<RouteMessageAck(
        const RouteMessageWrapper&, RoutingMode, std::optional<DhtAddress>)>
        doRouteMessage;
    std::function<bool(const std::string&)> isMostLikelyDuplicate;
    std::function<void(const std::string&)> addToDuplicateDetector;
};

class RecursiveOperationManager : public EnableSharedFromThis {
private:
    static constexpr size_t closestConnectedNodesCount = 5;
    static constexpr std::chrono::milliseconds sessionRpcTimeout{10000};
    static constexpr std::chrono::milliseconds deleteWaitTime{50};

    RecursiveOperationManagerOptions options;
    std::recursive_mutex mutex;
    std::map<std::string, std::shared_ptr<RecursiveOperationSession>>
        ongoingSessions;
    bool stopped = false;
    std::unique_ptr<RecursiveOperationRpcLocal> rpcLocal;

    explicit RecursiveOperationManager(RecursiveOperationManagerOptions options)
        : options(std::move(options)) {}

    void registerLocalRpcMethods() {
        this->rpcLocal = std::make_unique<RecursiveOperationRpcLocal>(
            RecursiveOperationRpcLocalOptions{
                .doRouteRequest =
                    [this](const RouteMessageWrapper& routedMessage) {
                        return this->doRouteRequest(
                            routedMessage, std::nullopt);
                    },
                .addContact =
                    [this](const PeerDescriptor& contact, bool /*setActive*/) {
                        this->options.addContact(contact);
                    },
                .isMostLikelyDuplicate =
                    [this](const std::string& requestId) {
                        return this->options.isMostLikelyDuplicate(requestId);
                    },
                .addToDuplicateDetector =
                    [this](const std::string& requestId) {
                        this->options.addToDuplicateDetector(requestId);
                    }});
        std::weak_ptr<RecursiveOperationManager> weakSelf =
            this->sharedFromThis<RecursiveOperationManager>();
        this->options.rpcCommunicator
            .template registerRpcMethod<RouteMessageWrapper, RouteMessageAck>(
                "routeRequest",
                [weakSelf](
                    const RouteMessageWrapper& routedMessage,
                    const DhtCallContext& callContext) -> RouteMessageAck {
                    auto self = weakSelf.lock();
                    if (!self || self->stopped) {
                        return createRouteMessageAck(
                            routedMessage, RouteMessageError::STOPPED);
                    }
                    return self->rpcLocal->routeRequest(
                        routedMessage, callContext);
                });
    }

    [[nodiscard]] std::vector<PeerDescriptor> getClosestConnectedNodes(
        const DhtAddress& referenceId, size_t limit) {
        std::vector<std::shared_ptr<DhtNodeRpcRemote>> connectedNodes;
        for (const auto& connection :
             this->options.connectionsView.getConnections()) {
            connectedNodes.push_back(
                this->options.createDhtNodeRpcRemote(connection));
        }
        SortedContactList<DhtNodeRpcRemote> sorted(
            SortedContactListOptions{
                .referenceId = referenceId,
                .allowToContainReferenceId = true,
                .maxSize = limit});
        sorted.addContacts(connectedNodes);
        std::vector<PeerDescriptor> result;
        for (const auto& peer :
             sorted.getClosestContacts(static_cast<int>(limit))) {
            result.push_back(peer->getPeerDescriptor());
        }
        return result;
    }

    [[nodiscard]] bool isPeerCloserToIdThanSelf(
        const PeerDescriptor& peer, const DhtAddress& nodeIdOrDataKey) {
        const DhtAddressRaw raw =
            Identifiers::getRawFromDhtAddress(nodeIdOrDataKey);
        const double distance1 =
            getPeerDistance(DhtAddressRaw{peer.nodeid()}, raw);
        const double distance2 = getPeerDistance(
            DhtAddressRaw{this->options.localPeerDescriptor.nodeid()}, raw);
        return distance1 < distance2;
    }

    void sendResponse(
        const std::vector<PeerDescriptor>& routingPath,
        const PeerDescriptor& targetPeerDescriptor,
        const ServiceID& serviceId,
        const std::vector<PeerDescriptor>& closestConnectedNodes,
        const std::vector<DataEntry>& dataEntries,
        bool noCloserNodesFound) {
        const bool isOwnNode = Identifiers::areEqualPeerDescriptors(
            this->options.localPeerDescriptor, targetPeerDescriptor);
        const auto it = this->ongoingSessions.find(serviceId);
        if (isOwnNode && it != this->ongoingSessions.end()) {
            it->second->onResponseReceived(
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->options.localPeerDescriptor),
                routingPath,
                closestConnectedNodes,
                dataEntries,
                noCloserNodesFound);
        } else {
            ListeningRpcCommunicator remoteCommunicator(
                ServiceID{serviceId},
                this->options.sessionTransport,
                RpcCommunicatorOptions{.rpcRequestTimeout = sessionRpcTimeout});
            RecursiveOperationSessionRpcRemote rpcRemote(
                this->options.localPeerDescriptor,
                targetPeerDescriptor,
                RecursiveOperationSessionRpcClient(remoteCommunicator),
                sessionRpcTimeout);
            rpcRemote.sendResponse(
                routingPath,
                closestConnectedNodes,
                dataEntries,
                noCloserNodesFound);
            remoteCommunicator.destroy();
        }
    }

    RouteMessageAck doRouteRequest(
        const RouteMessageWrapper& routedMessage,
        std::optional<DhtAddress> excludedPeer) {
        std::scoped_lock lock(this->mutex);
        if (this->stopped) {
            return createRouteMessageAck(
                routedMessage, RouteMessageError::STOPPED);
        }
        if (!routedMessage.message().has_recursiveoperationrequest()) {
            throw std::runtime_error(
                "routeRequest payload is not a RecursiveOperationRequest");
        }
        const DhtAddress targetId = Identifiers::getDhtAddressFromRaw(
            DhtAddressRaw{routedMessage.target()});
        const RecursiveOperationRequest& request =
            routedMessage.message().recursiveoperationrequest();
        const auto closestConnectedNodes = this->getClosestConnectedNodes(
            targetId, closestConnectedNodesCount);
        std::vector<DataEntry> dataEntries;
        if (request.operation() == RecursiveOperation::FETCH_DATA) {
            dataEntries = this->options.localDataStore.values(targetId);
        }
        if (request.operation() == RecursiveOperation::DELETE_DATA) {
            this->options.localDataStore.markAsDeleted(
                targetId,
                Identifiers::getNodeIdFromPeerDescriptor(
                    routedMessage.sourcepeer()));
        }
        const std::vector<PeerDescriptor> routingPath(
            routedMessage.routingpath().begin(),
            routedMessage.routingpath().end());
        if (this->options.localPeerDescriptor.nodeid() ==
            routedMessage.target()) {
            this->sendResponse(
                routingPath,
                routedMessage.sourcepeer(),
                ServiceID{request.sessionid()},
                closestConnectedNodes,
                dataEntries,
                true);
            return createRouteMessageAck(routedMessage);
        }
        const RouteMessageAck ack = this->options.doRouteMessage(
            routedMessage, RoutingMode::RECURSIVE, excludedPeer);
        if (!ack.has_error() || ack.error() == RouteMessageError::NO_TARGETS) {
            const bool noCloserContactsFound =
                (ack.has_error() &&
                 ack.error() == RouteMessageError::NO_TARGETS) ||
                (!closestConnectedNodes.empty() &&
                 getPreviousPeer(routedMessage).has_value() &&
                 !this->isPeerCloserToIdThanSelf(
                     closestConnectedNodes[0], targetId));
            this->sendResponse(
                routingPath,
                routedMessage.sourcepeer(),
                ServiceID{request.sessionid()},
                closestConnectedNodes,
                dataEntries,
                noCloserContactsFound);
        }
        return ack;
    }

public:
    [[nodiscard]] static std::shared_ptr<RecursiveOperationManager> newInstance(
        RecursiveOperationManagerOptions options) {
        struct MakeSharedEnabler : public RecursiveOperationManager {
            explicit MakeSharedEnabler(RecursiveOperationManagerOptions options)
                : RecursiveOperationManager(std::move(options)) {}
        };
        auto instance = std::make_shared<MakeSharedEnabler>(std::move(options));
        instance->registerLocalRpcMethods();
        return instance;
    }

    ~RecursiveOperationManager() override = default;

    folly::coro::Task<RecursiveOperationResult> execute(
        DhtAddress targetId,
        RecursiveOperation operation,
        std::optional<DhtAddress> excludedPeer = std::nullopt,
        bool waitForCompletion = true) {
        {
            std::scoped_lock lock(this->mutex);
            if (this->stopped) {
                co_return RecursiveOperationResult{};
            }
        }
        auto self = this->sharedFromThis<RecursiveOperationManager>();
        const size_t waitedRoutingPathCompletions =
            this->options.connectionsView.getConnectionCount() > 1 ? 2 : 1;
        auto session = RecursiveOperationSession::newInstance(
            RecursiveOperationSessionOptions{
                .transport = this->options.sessionTransport,
                .targetId = targetId,
                .localPeerDescriptor = this->options.localPeerDescriptor,
                .waitedRoutingPathCompletions = waitedRoutingPathCompletions,
                .operation = operation,
                .doRouteRequest =
                    [self, excludedPeer](const RouteMessageWrapper& message) {
                        return self->doRouteRequest(message, excludedPeer);
                    }});
        if (this->options.connectionsView.getConnectionCount() == 0) {
            const auto dataEntries =
                this->options.localDataStore.values(targetId);
            session->onResponseReceived(
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->options.localPeerDescriptor),
                {this->options.localPeerDescriptor},
                {this->options.localPeerDescriptor},
                dataEntries,
                true);
            co_return session->getResults();
        }
        {
            std::scoped_lock lock(this->mutex);
            this->ongoingSessions.emplace(session->getId(), session);
        }
        if (waitForCompletion) {
            session->start(this->options.serviceId);
            if (!session->isCompletionEmitted()) {
                try {
                    co_await streamr::utils::waitForEvent<
                        recursiveoperationsessionevents::Completed>(
                        session.get(), recursiveOperationTimeout);
                } catch (const std::exception& err) {
                    SLogger::debug("start failed " + std::string(err.what()));
                }
            }
        } else {
            session->start(this->options.serviceId);
            // Give the router time to send the delete out.
            co_await folly::coro::sleep(deleteWaitTime);
        }
        if (operation == RecursiveOperation::FETCH_DATA) {
            const auto dataEntries =
                this->options.localDataStore.values(targetId);
            if (!dataEntries.empty()) {
                this->sendResponse(
                    {this->options.localPeerDescriptor},
                    this->options.localPeerDescriptor,
                    ServiceID{session->getId()},
                    {},
                    dataEntries,
                    true);
            }
        } else if (operation == RecursiveOperation::DELETE_DATA) {
            this->options.localDataStore.markAsDeleted(
                targetId,
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->options.localPeerDescriptor));
        }
        {
            std::scoped_lock lock(this->mutex);
            this->ongoingSessions.erase(session->getId());
        }
        session->stop();
        co_return session->getResults();
    }

    void stop() {
        std::scoped_lock lock(this->mutex);
        this->stopped = true;
        for (auto& [sessionId, session] : this->ongoingSessions) {
            session->stop();
        }
        this->ongoingSessions.clear();
    }
};

} // namespace streamr::dht::recursiveoperation
