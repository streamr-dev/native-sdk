// Module streamr.dht.RecursiveOperationSession
// Ported from packages/dht/src/dht/recursive-operation/
// RecursiveOperationSession.ts (v103.8.0-rc.3). The initiator's side of a
// recursive operation: it starts the routed request, collects the
// per-hop RecursiveOperationResponse reports (closest nodes + any data),
// and emits `completed` once every routing path has reported back (or a
// fallback timeout fires).
//
// RecursiveOperationResult is defined here (TS keeps it in
// RecursiveOperationManager) to break the Session <-> Manager import cycle
// that C++ modules cannot express.
module;

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

export module streamr.dht.RecursiveOperationSession;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.utils.AbortController;
import streamr.utils.AbortableTimers;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.Uuid;
import streamr.logger.SLogger;
import streamr.protorpc.RpcCommunicator;
import streamr.dht.Contact;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.RecursiveOperationSessionRpcLocal;
import streamr.dht.SortedContactList;
import streamr.dht.Transport;

// Hoisted from the former header (file scope, NOT exported).
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::AbortableTimers;
using streamr::utils::AbortController;
using streamr::utils::EnableSharedFromThis;
using streamr::utils::Uuid;

export namespace streamr::dht::recursiveoperation {

using ::dht::DataEntry;
using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperation;
using ::dht::RecursiveOperationRequest;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageWrapper;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::contact::Contact;
using streamr::dht::contact::SortedContactList;
using streamr::dht::contact::SortedContactListOptions;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;
using streamr::protorpc::RpcCommunicatorOptions;

struct RecursiveOperationResult {
    std::vector<PeerDescriptor> closestNodes;
    std::vector<DataEntry> dataEntries;
};

namespace recursiveoperationsessionevents {
struct Completed : Event<> {};
} // namespace recursiveoperationsessionevents

using RecursiveOperationSessionEvents =
    std::tuple<recursiveoperationsessionevents::Completed>;

inline constexpr std::chrono::milliseconds recursiveOperationTimeout{10000};

struct RecursiveOperationSessionOptions {
    Transport& transport;
    DhtAddress targetId;
    PeerDescriptor localPeerDescriptor;
    size_t waitedRoutingPathCompletions;
    RecursiveOperation operation;
    std::function<RouteMessageAck(const RouteMessageWrapper&)> doRouteRequest;
};

class RecursiveOperationSession
    : public EventEmitter<RecursiveOperationSessionEvents>,
      public EnableSharedFromThis {
private:
    static constexpr int resultsMaxSize = 10;
    static constexpr std::chrono::milliseconds noCloserNodesTimeout{4000};

    RecursiveOperationSessionOptions options;
    std::string id = Uuid::v4();
    std::recursive_mutex mutex;
    ListeningRpcCommunicator rpcCommunicator;
    SortedContactList<Contact> results;
    std::map<DhtAddress, DataEntry> foundData;
    std::set<DhtAddress> allKnownHops;
    std::set<DhtAddress> reportedHops;
    std::set<DhtAddress> noCloserNodesReceivedFrom;
    AbortController abortController; // cancels the fallback timeout on stop
    bool completionEventEmitted = false;
    bool timeoutScheduled = false;
    int noCloserNodesReceivedCounter = 0;

    explicit RecursiveOperationSession(RecursiveOperationSessionOptions options)
        : options(std::move(options)),
          rpcCommunicator(
              ServiceID{this->id},
              this->options.transport,
              RpcCommunicatorOptions{
                  .rpcRequestTimeout = recursiveOperationTimeout}),
          results(
              SortedContactList<Contact>(SortedContactListOptions{
                  .referenceId = this->options.targetId,
                  .allowToContainReferenceId = true,
                  .maxSize = static_cast<size_t>(resultsMaxSize)})) {}

    void registerLocalRpcMethods() {
        auto rpcLocal = std::make_shared<RecursiveOperationSessionRpcLocal>(
            RecursiveOperationSessionRpcLocalOptions{
                .onResponseReceived =
                    [this](
                        const DhtAddress& remoteNodeId,
                        const std::vector<PeerDescriptor>& routingPath,
                        const std::vector<PeerDescriptor>&
                            closestConnectedNodes,
                        const std::vector<DataEntry>& dataEntries,
                        bool noCloserNodesFound) {
                        this->onResponseReceived(
                            remoteNodeId,
                            routingPath,
                            closestConnectedNodes,
                            dataEntries,
                            noCloserNodesFound);
                    }});
        this->rpcCommunicator
            .registerRpcNotification<::dht::RecursiveOperationResponse>(
                "sendResponse",
                [rpcLocal](
                    const ::dht::RecursiveOperationResponse& request,
                    const streamr::dht::rpcprotocol::DhtCallContext& context) {
                    rpcLocal->sendResponse(request, context);
                });
    }

    [[nodiscard]] bool hasNonStaleData() const {
        for (const auto& [creator, entry] : this->foundData) {
            if (!entry.stale()) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool isCompleted() const {
        std::set<DhtAddress> unreportedHops = this->allKnownHops;
        for (const auto& id : this->reportedHops) {
            unreportedHops.erase(id);
        }
        if (this->noCloserNodesReceivedCounter >= 1 && unreportedHops.empty()) {
            if (this->options.operation == RecursiveOperation::FETCH_DATA &&
                (this->hasNonStaleData() ||
                 this->noCloserNodesReceivedCounter >=
                     static_cast<int>(
                         this->options.waitedRoutingPathCompletions))) {
                return true;
            }
            if (this->options.operation == RecursiveOperation::FETCH_DATA) {
                return false;
            }
            return true;
        }
        return false;
    }

    void addKnownHops(const std::vector<PeerDescriptor>& routingPath) {
        const DhtAddress localNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            this->options.localPeerDescriptor);
        for (const auto& descriptor : routingPath) {
            const DhtAddress newNodeId =
                Identifiers::getNodeIdFromPeerDescriptor(descriptor);
            if (localNodeId != newNodeId) {
                this->allKnownHops.insert(newNodeId);
            }
        }
    }

    void emitCompleted() {
        this->abortController.abort();
        this->completionEventEmitted = true;
        this->emit<recursiveoperationsessionevents::Completed>();
    }

    void setHopAsReported(const PeerDescriptor& descriptor) {
        const DhtAddress localNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            this->options.localPeerDescriptor);
        const DhtAddress newNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(descriptor);
        if (localNodeId != newNodeId) {
            this->reportedHops.insert(newNodeId);
        }
        if (!this->completionEventEmitted && this->isCompleted()) {
            this->emitCompleted();
        }
    }

    static bool isNewerTimestamp(
        const ::google::protobuf::Timestamp& candidate,
        const ::google::protobuf::Timestamp& existing) {
        if (candidate.seconds() != existing.seconds()) {
            return candidate.seconds() > existing.seconds();
        }
        return candidate.nanos() > existing.nanos();
    }

    void processFoundData(const std::vector<DataEntry>& dataEntries) {
        for (const auto& entry : dataEntries) {
            const DhtAddress creatorNodeId = Identifiers::getDhtAddressFromRaw(
                DhtAddressRaw{entry.creator()});
            const auto it = this->foundData.find(creatorNodeId);
            if (it == this->foundData.end() ||
                isNewerTimestamp(entry.createdat(), it->second.createdat()) ||
                (!isNewerTimestamp(it->second.createdat(), entry.createdat()) &&
                 entry.deleted())) {
                this->foundData.insert_or_assign(creatorNodeId, entry);
            }
        }
    }

    void onNoCloserPeersFound(const DhtAddress& remoteNodeId) {
        this->noCloserNodesReceivedCounter += 1;
        this->noCloserNodesReceivedFrom.insert(remoteNodeId);
        if (this->isCompleted()) {
            this->emitCompleted();
        } else if (!this->timeoutScheduled && !this->completionEventEmitted) {
            this->timeoutScheduled = true;
            auto self = this->sharedFromThis<RecursiveOperationSession>();
            AbortableTimers::setAbortableTimeout(
                [self]() {
                    std::scoped_lock lock(self->mutex);
                    if (!self->completionEventEmitted) {
                        self->emitCompleted();
                    }
                },
                noCloserNodesTimeout,
                this->abortController.getSignal());
        }
    }

    RouteMessageWrapper wrapRequest(const ServiceID& serviceId) {
        RecursiveOperationRequest request;
        request.set_sessionid(this->id);
        request.set_operation(this->options.operation);
        Message msg;
        msg.set_messageid(Uuid::v4());
        msg.set_serviceid(serviceId);
        *msg.mutable_recursiveoperationrequest() = request;
        RouteMessageWrapper routeMessage;
        *routeMessage.mutable_message() = msg;
        routeMessage.set_requestid(Uuid::v4());
        routeMessage.set_target(
            Identifiers::getRawFromDhtAddress(this->options.targetId));
        *routeMessage.mutable_sourcepeer() = this->options.localPeerDescriptor;
        return routeMessage;
    }

public:
    [[nodiscard]] static std::shared_ptr<RecursiveOperationSession> newInstance(
        RecursiveOperationSessionOptions options) {
        struct MakeSharedEnabler : public RecursiveOperationSession {
            explicit MakeSharedEnabler(RecursiveOperationSessionOptions options)
                : RecursiveOperationSession(std::move(options)) {}
        };
        auto instance = std::make_shared<MakeSharedEnabler>(std::move(options));
        instance->registerLocalRpcMethods();
        return instance;
    }

    ~RecursiveOperationSession() override = default;

    void start(const ServiceID& serviceId) {
        std::scoped_lock lock(this->mutex);
        const auto routeMessage = this->wrapRequest(serviceId);
        this->options.doRouteRequest(routeMessage);
    }

    void onResponseReceived(
        const DhtAddress& remoteNodeId,
        const std::vector<PeerDescriptor>& routingPath,
        const std::vector<PeerDescriptor>& closestConnectedNodes,
        const std::vector<DataEntry>& dataEntries,
        bool noCloserNodesFound) {
        std::scoped_lock lock(this->mutex);
        this->addKnownHops(routingPath);
        if (!routingPath.empty()) {
            this->setHopAsReported(routingPath.back());
        }
        for (const auto& descriptor : closestConnectedNodes) {
            this->results.addContact(std::make_shared<Contact>(descriptor));
        }
        this->processFoundData(dataEntries);
        if (noCloserNodesFound ||
            this->noCloserNodesReceivedFrom.contains(remoteNodeId)) {
            this->onNoCloserPeersFound(remoteNodeId);
        }
    }

    [[nodiscard]] RecursiveOperationResult getResults() {
        std::scoped_lock lock(this->mutex);
        RecursiveOperationResult result;
        for (const auto& contact : this->results.getClosestContacts()) {
            result.closestNodes.push_back(contact->getPeerDescriptor());
        }
        for (const auto& [creator, entry] : this->foundData) {
            result.dataEntries.push_back(entry);
        }
        return result;
    }

    [[nodiscard]] const std::string& getId() const { return this->id; }

    // Lets the manager skip waiting when start() completed synchronously
    // (e.g. this node is the target), avoiding a missed 'completed' event.
    [[nodiscard]] bool isCompletionEmitted() {
        std::scoped_lock lock(this->mutex);
        return this->completionEventEmitted;
    }

    void stop() {
        {
            std::scoped_lock lock(this->mutex);
            this->abortController.abort();
        }
        // NOT under this->mutex: destroy() waits out an in-flight
        // transport Message handler, and that handler (onResponseReceived)
        // takes this->mutex — holding it here is an ABBA deadlock.
        this->rpcCommunicator.destroy();
        {
            std::scoped_lock lock(this->mutex);
            if (!this->completionEventEmitted) {
                this->completionEventEmitted = true;
                this->emit<recursiveoperationsessionevents::Completed>();
            }
        }
    }
};

} // namespace streamr::dht::recursiveoperation
