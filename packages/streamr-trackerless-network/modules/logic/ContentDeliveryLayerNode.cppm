// Module streamr.trackerlessnetwork.ContentDeliveryLayerNode
// Ported from packages/trackerless-network/src/content-delivery-layer/
// ContentDeliveryLayerNode.ts (v103.8.0-rc.3): the per-stream-part
// overlay node — owns the neighbor list and the four contact views fed
// from the discovery layer, broadcasts with per-publisher duplicate
// detection, and wires the content-delivery / temporary-connection RPC
// servers. The plumtree branch is milestone-E scope (plan decision 3.3)
// and the TS GapDiagnostics sampling is a TS-only diagnostic; both are
// omitted.
//
// Adaptations: components arrive as shared_ptrs (the TS factory relies
// on GC; here createContentDeliveryLayerNode builds the ownership graph
// and the options struct keeps referenced parts alive — view lists and
// the ongoing-handshake set are declared before the components that
// hold references into them, so they are destroyed last). The TS
// fire-and-forget leave notices in stop() are awaited as notifications
// (they complete at send-enqueue time and swallow errors).
module;

#include <coroutine> // IWYU pragma: keep

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

export module streamr.trackerlessnetwork.ContentDeliveryLayerNode;

import streamr.trackerlessnetwork.protos;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryRpcLocal;
import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.trackerlessnetwork.DuplicateMessageDetector;
import streamr.trackerlessnetwork.Handshaker;
import streamr.trackerlessnetwork.Inspector;
import streamr.trackerlessnetwork.NeighborFinder;
import streamr.trackerlessnetwork.NeighborUpdateManager;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.Propagation;
import streamr.trackerlessnetwork.ProxyConnectionRpcLocal;
import streamr.trackerlessnetwork.TemporaryConnectionRpcLocal;
import streamr.trackerlessnetwork.Utils;
import streamr.dht.ConnectionLocker;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.Transport;
import streamr.dht.protos;
import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;
import streamr.utils.StreamPartID;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionLocker;
using streamr::dht::connection::LockID;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using streamr::trackerlessnetwork::discoverylayer::DiscoveryLayerNode;
using streamr::trackerlessnetwork::inspection::Inspector;
using streamr::trackerlessnetwork::neighbordiscovery::Handshaker;
using streamr::trackerlessnetwork::neighbordiscovery::INeighborFinder;
using streamr::trackerlessnetwork::neighbordiscovery::NeighborUpdateManager;
using streamr::trackerlessnetwork::propagation::Propagation;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcLocal;

namespace contentdeliverylayernodeevents {

struct Message : Event<StreamMessage> {};
struct NeighborConnected : Event<DhtAddress> {};
struct EntryPointLeaveDetected : Event<> {};

} // namespace contentdeliverylayernodeevents

using ContentDeliveryLayerNodeEvents = std::tuple<
    contentdeliverylayernodeevents::Message,
    contentdeliverylayernodeevents::NeighborConnected,
    contentdeliverylayernodeevents::EntryPointLeaveDetected>;

constexpr size_t defaultNodeViewSize = 20;
constexpr size_t defaultNeighborTargetCount = 4;
constexpr bool defaultAcceptProxyConnections = false;

struct ContentDeliveryLayerNeighborInfo {
    PeerDescriptor peerDescriptor;
    std::optional<int64_t> rtt;
};

// Members referenced by other components (view lists, the
// ongoing-handshake set, the communicator) are declared FIRST so they
// outlive the components holding references into them.
struct StrictContentDeliveryLayerNodeOptions {
    StreamPartID streamPartId;
    std::shared_ptr<DiscoveryLayerNode> discoveryLayerNode;
    Transport* transport = nullptr;
    ConnectionLocker* connectionLocker = nullptr;
    PeerDescriptor localPeerDescriptor;
    size_t nodeViewSize = defaultNodeViewSize;
    std::shared_ptr<ListeningRpcCommunicator> rpcCommunicator;
    std::shared_ptr<std::set<DhtAddress>> ongoingHandshakes;
    std::shared_ptr<NodeList> nearbyNodeView;
    std::shared_ptr<NodeList> randomNodeView;
    std::shared_ptr<NodeList> leftNodeView;
    std::shared_ptr<NodeList> rightNodeView;
    std::shared_ptr<NodeList> neighbors;
    std::shared_ptr<TemporaryConnectionRpcLocal> temporaryConnectionRpcLocal;
    std::shared_ptr<ProxyConnectionRpcLocal>
        proxyConnectionRpcLocal; // may be null
    std::shared_ptr<Propagation> propagation;
    std::shared_ptr<Handshaker> handshaker;
    std::shared_ptr<INeighborFinder> neighborFinder;
    std::shared_ptr<NeighborUpdateManager> neighborUpdateManager;
    std::shared_ptr<Inspector> inspector;
    size_t neighborTargetCount = defaultNeighborTargetCount;
    std::function<bool()> isLocalNodeEntryPoint;
    std::optional<std::chrono::milliseconds> rpcRequestTimeout;
    bool suppressOwnMessageLoopback = false;
};

class ContentDeliveryLayerNode
    : public EventEmitter<ContentDeliveryLayerNodeEvents> {
private:
    StrictContentDeliveryLayerNodeOptions options;
    std::map<std::string, DuplicateMessageDetector> duplicateDetectors;
    std::optional<ContentDeliveryRpcLocal> contentDeliveryRpcLocal;
    std::atomic<bool> started = false;
    std::atomic<bool> stopped = false;
    std::vector<std::function<void()>> unsubscribers;
    std::atomic<uint64_t> messagesPropagated = 0;

public:
    explicit ContentDeliveryLayerNode(
        StrictContentDeliveryLayerNodeOptions options)
        : options(std::move(options)) {
        this->contentDeliveryRpcLocal.emplace(
            ContentDeliveryRpcLocalOptions{
                .localPeerDescriptor = this->options.localPeerDescriptor,
                .streamPartId = this->options.streamPartId,
                .markAndCheckDuplicate =
                    [this](
                        const MessageID& msg,
                        const std::optional<MessageRef>& prev) {
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
                        const DhtAddress& remoteNodeId,
                        bool sourceIsStreamEntryPoint) {
                        this->onLeaveNotice(
                            remoteNodeId, sourceIsStreamEntryPoint);
                    },
                .markForInspection =
                    [this](
                        const DhtAddress& remoteNodeId,
                        const MessageID& messageId) {
                        this->options.inspector->markMessage(
                            remoteNodeId, messageId);
                    },
                .rpcCommunicator = *this->options.rpcCommunicator});
    }

    ~ContentDeliveryLayerNode() override { this->stop(); }

    folly::coro::Task<void> start() {
        this->started = true;
        this->registerDefaultServerMethods();
        this->subscribeToSources();
        this->options.neighborFinder->start();
        this->options.neighborUpdateManager->start();
        co_return;
    }

    void stop() {
        if (!this->started || this->stopped.exchange(true)) {
            return;
        }
        for (const auto& unsubscribe : this->unsubscribers) {
            unsubscribe();
        }
        this->unsubscribers.clear();
        if (this->options.proxyConnectionRpcLocal) {
            this->options.proxyConnectionRpcLocal->stop();
        }
        {
            // TS fires these without awaiting; the notices are
            // notifications (complete at send-enqueue) and swallow
            // errors, so awaiting them here is bounded.
            // Pin the remotes for the whole batch: the lazy notice
            // coroutines capture each remote's `this`, and concurrent
            // handlers may drop the neighbor list's own references
            // before blockingWait starts the tasks (the 64-node
            // teardown SIGSEGV — the rpc-remote lazy-task trap).
            const auto remotes = this->options.neighbors->getAll();
            std::vector<folly::coro::Task<void>> notices;
            notices.reserve(remotes.size());
            for (const auto& remote : remotes) {
                notices.push_back(remote->leaveStreamPartNotice(
                    this->options.streamPartId,
                    this->options.isLocalNodeEntryPoint()));
                this->options.connectionLocker->weakUnlockConnection(
                    Identifiers::getNodeIdFromPeerDescriptor(
                        remote->getPeerDescriptor()),
                    LockID{this->options.streamPartId});
            }
            streamr::utils::blockingWait(
                folly::coro::collectAllTryRange(std::move(notices)));
        }
        // TS parity: destroy() unregisters from the transport and drains
        // the communicator scopes NOW, while the send targets are alive —
        // a straggler leave-notice coroutine resuming after this object
        // dies was the 64-node teardown SIGSEGV.
        this->options.rpcCommunicator->destroy();
        this->removeAllListeners();
        this->options.nearbyNodeView->stop();
        this->options.neighbors->stop();
        this->options.randomNodeView->stop();
        this->options.neighborFinder->stop();
        this->options.neighborUpdateManager->stop();
        this->options.inspector->stop();
        this->duplicateDetectors.clear();
    }

    void broadcast(
        const StreamMessage& msg,
        const std::optional<DhtAddress>& previousNode = std::nullopt) {
        if (!previousNode.has_value()) {
            Utils::markAndCheckDuplicate(
                this->duplicateDetectors,
                msg.messageid(),
                msg.has_previousmessageref()
                    ? std::optional(msg.previousmessageref())
                    : std::nullopt);
        }
        // Deliver to local listeners — except own publishes (no
        // previousNode) when loopback is suppressed: nothing local
        // consumes them. Propagation + duplicate detection are
        // unaffected.
        if (previousNode.has_value() ||
            !this->options.suppressOwnMessageLoopback) {
            this->emit<contentdeliverylayernodeevents::Message>(msg);
        }
        const bool skipBackPropagation = previousNode.has_value() &&
            !this->options.temporaryConnectionRpcLocal->hasNode(
                previousNode.value());
        this->options.propagation->feedUnseenMessage(
            msg,
            this->getPropagationTargets(msg),
            skipBackPropagation ? previousNode : std::nullopt);
        this->messagesPropagated++;
    }

    folly::coro::Task<bool> inspect(PeerDescriptor peerDescriptor) {
        return this->options.inspector->inspect(std::move(peerDescriptor));
    }

    [[nodiscard]] bool hasProxyConnection(const DhtAddress& nodeId) const {
        if (this->options.proxyConnectionRpcLocal) {
            return this->options.proxyConnectionRpcLocal->hasConnection(nodeId);
        }
        return false;
    }

    [[nodiscard]] DhtAddress getOwnNodeId() const {
        return Identifiers::getNodeIdFromPeerDescriptor(
            this->options.localPeerDescriptor);
    }

    [[nodiscard]] size_t getOutgoingHandshakeCount() const {
        return this->options.handshaker->getOngoingHandshakes().size();
    }

    [[nodiscard]] std::vector<PeerDescriptor> getNeighbors() const {
        if (!this->started && this->stopped) {
            return {};
        }
        std::vector<PeerDescriptor> descriptors;
        for (const auto& neighbor : this->options.neighbors->getAll()) {
            descriptors.push_back(neighbor->getPeerDescriptor());
        }
        return descriptors;
    }

    [[nodiscard]] std::vector<ContentDeliveryLayerNeighborInfo> getInfos()
        const {
        std::vector<ContentDeliveryLayerNeighborInfo> infos;
        for (const auto& neighbor : this->options.neighbors->getAll()) {
            infos.push_back(
                ContentDeliveryLayerNeighborInfo{
                    .peerDescriptor = neighbor->getPeerDescriptor(),
                    .rtt = neighbor->getRtt()});
        }
        return infos;
    }

    [[nodiscard]] NodeList& getNearbyNodeView() {
        return *this->options.nearbyNodeView;
    }

private:
    void registerDefaultServerMethods() {
        this->options.rpcCommunicator->registerRpcNotification<StreamMessage>(
            "sendStreamMessage",
            [this](const StreamMessage& msg, const DhtCallContext& context) {
                this->contentDeliveryRpcLocal->sendStreamMessage(msg, context);
            });
        this->options.rpcCommunicator
            ->registerRpcNotification<LeaveStreamPartNotice>(
                "leaveStreamPartNotice",
                [this](
                    const LeaveStreamPartNotice& req,
                    const DhtCallContext& context) {
                    this->contentDeliveryRpcLocal->leaveStreamPartNotice(
                        req, context);
                });
        this->options.rpcCommunicator->registerRpcMethod<
            TemporaryConnectionRequest,
            TemporaryConnectionResponse>(
            "openConnection",
            [this](
                const TemporaryConnectionRequest& req,
                const DhtCallContext& context) {
                return this->options.temporaryConnectionRpcLocal
                    ->openConnection(req, context);
            });
        this->options.rpcCommunicator
            ->registerRpcNotification<CloseTemporaryConnection>(
                "closeConnection",
                [this](
                    const CloseTemporaryConnection& req,
                    const DhtCallContext& context) {
                    this->options.temporaryConnectionRpcLocal->closeConnection(
                        req, context);
                });
    }

    template <typename EventType, typename EmitterType, typename Handler>
    void subscribe(EmitterType& source, Handler handler) {
        const auto token = source.template on<EventType>(std::move(handler));
        this->unsubscribers.emplace_back(
            [&source, token]() { source.template off<EventType>(token); });
    }

    void subscribeToSources() {
        namespace dle = streamr::trackerlessnetwork::discoverylayer::
            discoverylayernodeevents;
        auto& discovery = *this->options.discoveryLayerNode;
        this->subscribe<dle::NearbyContactAdded>(
            discovery, [this](const PeerDescriptor& /*peer*/) {
                this->onNearbyContactAdded();
            });
        this->subscribe<dle::NearbyContactRemoved>(
            discovery, [this](const PeerDescriptor& /*peer*/) {
                this->onNearbyContactRemoved();
            });
        this->subscribe<dle::RandomContactAdded>(
            discovery, [this](const PeerDescriptor& /*peer*/) {
                this->onRandomContactAdded();
            });
        this->subscribe<dle::RandomContactRemoved>(
            discovery, [this](const PeerDescriptor& /*peer*/) {
                this->onRandomContactRemoved();
            });
        this->subscribe<dle::RingContactAdded>(
            discovery, [this](const PeerDescriptor& /*peer*/) {
                this->onRingContactsUpdated();
            });
        this->subscribe<dle::RingContactRemoved>(
            discovery, [this](const PeerDescriptor& /*peer*/) {
                this->onRingContactsUpdated();
            });
        this->subscribe<streamr::dht::transport::transportevents::Disconnected>(
            *this->options.transport,
            [this](
                const PeerDescriptor& peerDescriptor, bool /*gracefulLeave*/) {
                this->onNodeDisconnected(peerDescriptor);
            });
        this->subscribe<NodeAdded>(
            *this->options.neighbors,
            [this](
                const DhtAddress& id,
                const std::shared_ptr<ContentDeliveryRpcRemote>& remote) {
                this->options.propagation->onNeighborJoined(id);
                this->options.connectionLocker->weakLockConnection(
                    Identifiers::getNodeIdFromPeerDescriptor(
                        remote->getPeerDescriptor()),
                    LockID{this->options.streamPartId});
                this->emit<contentdeliverylayernodeevents::NeighborConnected>(
                    id);
            });
        this->subscribe<NodeRemoved>(
            *this->options.neighbors,
            [this](
                const DhtAddress& /*id*/,
                const std::shared_ptr<ContentDeliveryRpcRemote>& remote) {
                this->options.connectionLocker->weakUnlockConnection(
                    Identifiers::getNodeIdFromPeerDescriptor(
                        remote->getPeerDescriptor()),
                    LockID{this->options.streamPartId});
            });
        if (this->options.proxyConnectionRpcLocal) {
            this->subscribe<proxy::NewConnection>(
                *this->options.proxyConnectionRpcLocal,
                [this](const DhtAddress& id) {
                    this->options.propagation->onNeighborJoined(id);
                });
        }
    }

    void onLeaveNotice(
        const DhtAddress& remoteNodeId, bool sourceIsStreamEntryPoint) {
        if (this->stopped) {
            return;
        }
        const bool isKnown =
            this->options.nearbyNodeView->get(remoteNodeId).has_value() ||
            this->options.randomNodeView->get(remoteNodeId).has_value() ||
            this->options.neighbors->get(remoteNodeId).has_value() ||
            (this->options.proxyConnectionRpcLocal != nullptr &&
             this->options.proxyConnectionRpcLocal->getConnection(remoteNodeId)
                 .has_value());
        // TS TODO preserved: check integrity of notifier?
        if (isKnown) {
            this->options.discoveryLayerNode->removeContact(remoteNodeId);
            this->options.neighbors->remove(remoteNodeId);
            this->options.nearbyNodeView->remove(remoteNodeId);
            this->options.randomNodeView->remove(remoteNodeId);
            this->options.leftNodeView->remove(remoteNodeId);
            this->options.rightNodeView->remove(remoteNodeId);
            this->options.neighborFinder->start({remoteNodeId});
            if (this->options.proxyConnectionRpcLocal) {
                this->options.proxyConnectionRpcLocal->removeConnection(
                    remoteNodeId);
            }
        }
        if (sourceIsStreamEntryPoint) {
            this->emit<
                contentdeliverylayernodeevents::EntryPointLeaveDetected>();
        }
    }

    std::shared_ptr<ContentDeliveryRpcRemote> createRemote(
        const PeerDescriptor& peer) {
        ContentDeliveryRpcClient client{*this->options.rpcCommunicator};
        return std::make_shared<ContentDeliveryRpcRemote>(
            this->options.localPeerDescriptor,
            peer,
            client,
            this->options.rpcRequestTimeout);
    }

    void onRingContactsUpdated() {
        SLogger::trace("onRingContactsUpdated");
        if (this->stopped) {
            return;
        }
        const auto contacts =
            this->options.discoveryLayerNode->getRingContacts();
        std::vector<std::shared_ptr<ContentDeliveryRpcRemote>> left;
        for (const auto& peer : contacts.left) {
            left.push_back(this->createRemote(peer));
        }
        this->options.leftNodeView->replaceAll(left);
        std::vector<std::shared_ptr<ContentDeliveryRpcRemote>> right;
        for (const auto& peer : contacts.right) {
            right.push_back(this->createRemote(peer));
        }
        this->options.rightNodeView->replaceAll(right);
    }

    void onNearbyContactAdded() {
        SLogger::trace("New nearby contact found");
        if (this->stopped) {
            return;
        }
        this->updateNearbyNodeView(
            this->options.discoveryLayerNode->getClosestContacts(
                this->options.nodeViewSize));
        if (this->options.neighbors->size() <
            this->options.neighborTargetCount) {
            this->options.neighborFinder->start();
        }
    }

    void onNearbyContactRemoved() {
        SLogger::trace("Nearby contact removed");
        if (this->stopped) {
            return;
        }
        this->updateNearbyNodeView(
            this->options.discoveryLayerNode->getClosestContacts(
                this->options.nodeViewSize));
    }

    void updateNearbyNodeView(const std::vector<PeerDescriptor>& nodes) {
        std::vector<std::shared_ptr<ContentDeliveryRpcRemote>> remotes;
        for (const auto& descriptor : nodes) {
            remotes.push_back(this->createRemote(descriptor));
        }
        this->options.nearbyNodeView->replaceAll(remotes);
        for (const auto& descriptor :
             this->options.discoveryLayerNode->getNeighbors()) {
            if (this->options.nearbyNodeView->size() >=
                this->options.nodeViewSize) {
                break;
            }
            this->options.nearbyNodeView->add(this->createRemote(descriptor));
        }
    }

    void onRandomContactAdded() {
        if (this->stopped) {
            return;
        }
        this->replaceRandomNodeView();
        if (this->options.neighbors->size() <
            this->options.neighborTargetCount) {
            this->options.neighborFinder->start();
        }
    }

    void onRandomContactRemoved() {
        SLogger::trace("New random contact removed");
        if (this->stopped) {
            return;
        }
        this->replaceRandomNodeView();
    }

    void replaceRandomNodeView() {
        const auto randomContacts =
            this->options.discoveryLayerNode->getRandomContacts(
                this->options.nodeViewSize);
        std::vector<std::shared_ptr<ContentDeliveryRpcRemote>> remotes;
        for (const auto& descriptor : randomContacts) {
            remotes.push_back(this->createRemote(descriptor));
        }
        this->options.randomNodeView->replaceAll(remotes);
    }

    void onNodeDisconnected(const PeerDescriptor& peerDescriptor) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        if (this->options.neighbors->has(nodeId)) {
            this->options.neighbors->remove(nodeId);
            this->options.neighborFinder->start({nodeId});
            this->options.temporaryConnectionRpcLocal->removeNode(nodeId);
        }
    }

    std::vector<DhtAddress> getPropagationTargets(const StreamMessage& msg) {
        auto propagationTargets = this->options.neighbors->getIds();
        if (this->options.proxyConnectionRpcLocal) {
            const auto proxyTargets =
                this->options.proxyConnectionRpcLocal->getPropagationTargets(
                    msg);
            propagationTargets.insert(
                propagationTargets.end(),
                proxyTargets.begin(),
                proxyTargets.end());
        }
        const auto temporaryTargets =
            this->options.temporaryConnectionRpcLocal->getNodes().getIds();
        propagationTargets.insert(
            propagationTargets.end(),
            temporaryTargets.begin(),
            temporaryTargets.end());
        return propagationTargets;
    }
};

} // namespace streamr::trackerlessnetwork
