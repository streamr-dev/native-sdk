// Module streamr.trackerlessnetwork.HandshakeRpcLocal
// Ported from packages/trackerless-network/src/content-delivery-layer/
// neighbor-discovery/HandshakeRpcLocal.ts (v103.8.0-rc.3): the server
// side of stream-part neighbor handshakes, including the interleaving
// protocol (when full, accept the requester anyway and hand one existing
// neighbor over to it).
//
// Adaptation: TS fires the interleaveRequest continuation detached
// (".then() without await"); here it is a task on a GuardedAsyncScope
// drained on destruction. The task is time-bounded (the remote call
// carries interleaveRequestTimeout and swallows errors), satisfying the
// every-scope-task-must-be-bounded rule.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.HandshakeRpcLocal;

import streamr.utils.CoroutineHelper;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;
import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.HandshakeRpcRemote;
import streamr.trackerlessnetwork.NodeList;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.logger.SLogger;
import streamr.utils.StreamPartID;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::logger::SLogger;
using streamr::trackerlessnetwork::ContentDeliveryRpcRemote;
using streamr::utils::GuardedAsyncScope;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork::neighbordiscovery {

using ::dht::PeerDescriptor;

struct HandshakeRpcLocalOptions {
    StreamPartID streamPartId;
    NodeList& neighbors;
    std::set<DhtAddress>& ongoingHandshakes;
    std::set<DhtAddress>& ongoingInterleaves;
    size_t maxNeighborCount;
    std::function<std::shared_ptr<HandshakeRpcRemote>(const PeerDescriptor&)>
        createRpcRemote;
    std::function<std::shared_ptr<ContentDeliveryRpcRemote>(
        const PeerDescriptor&)>
        createContentDeliveryRpcRemote;
    std::function<folly::coro::Task<bool>(PeerDescriptor, DhtAddress)>
        handshakeWithInterleaving;
};

class HandshakeRpcLocal {
private:
    HandshakeRpcLocalOptions options;
    // Drains the detached interleave continuations (bounded by the
    // remote call's timeout) before the members they mutate go away.
    streamr::utils::SharedSerialExecutor interleaveExecutor{
        streamr::utils::SharedExecutors::worker()};
    GuardedAsyncScope interleaveScope;

public:
    explicit HandshakeRpcLocal(HandshakeRpcLocalOptions options)
        : options(std::move(options)) {}

    ~HandshakeRpcLocal() { this->interleaveScope.close(); }

    StreamPartHandshakeResponse handshake(
        const StreamPartHandshakeRequest& request,
        const DhtCallContext& context) {
        return this->handleRequest(request, context);
    }

    folly::coro::Task<InterleaveResponse> interleaveRequest(
        InterleaveRequest message, DhtCallContext context) {
        const auto senderPeerDescriptor =
            context.incomingSourceDescriptor.value();
        const auto remoteNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(senderPeerDescriptor);
        InterleaveResponse response;
        try {
            co_await this->options.handshakeWithInterleaving(
                message.interleavetargetdescriptor(), remoteNodeId);
            this->options.neighbors.remove(remoteNodeId);
            response.set_accepted(true);
        } catch (const std::exception& err) {
            SLogger::debug(
                "interleaveRequest to " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    message.interleavetargetdescriptor()) +
                " failed: " + std::string(err.what()));
            response.set_accepted(false);
        }
        co_return response;
    }

private:
    StreamPartHandshakeResponse handleRequest(
        const StreamPartHandshakeRequest& request,
        const DhtCallContext& context) {
        const auto senderDescriptor = context.incomingSourceDescriptor.value();
        const auto senderNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(senderDescriptor);
        std::vector<DhtAddress> interleaveNodeIds;
        if (request.has_interleavenodeid()) {
            interleaveNodeIds.push_back(
                Identifiers::getDhtAddressFromRaw(
                    DhtAddressRaw{request.interleavenodeid()}));
        }
        if (this->options.ongoingInterleaves.contains(senderNodeId)) {
            return rejectHandshake(request);
        }
        if (this->options.neighbors.has(senderNodeId) ||
            this->options.ongoingHandshakes.contains(senderNodeId)) {
            return this->acceptHandshake(request, senderDescriptor);
        }
        if (this->options.neighbors.size() +
                this->options.ongoingHandshakes.size() <
            this->options.maxNeighborCount) {
            return this->acceptHandshake(request, senderDescriptor);
        }
        if (this->options.neighbors.size(interleaveNodeIds) -
                    this->options.ongoingInterleaves.size() >=
                2 &&
            this->options.neighbors.size() <= this->options.maxNeighborCount) {
            // Do not accept the handshake requests if the target neighbor
            // count can potentially drop below 2 due to interleaving.
            // This ensures that a stable number of connections is kept
            // during high churn.
            return this->acceptHandshakeWithInterleaving(
                request, senderDescriptor);
        }
        return rejectHandshake(request);
    }

    StreamPartHandshakeResponse acceptHandshake(
        const StreamPartHandshakeRequest& request,
        const PeerDescriptor& requester) {
        StreamPartHandshakeResponse response;
        response.set_requestid(request.requestid());
        response.set_accepted(true);
        this->options.neighbors.add(
            this->options.createContentDeliveryRpcRemote(requester));
        return response;
    }

    static StreamPartHandshakeResponse rejectHandshake(
        const StreamPartHandshakeRequest& request) {
        StreamPartHandshakeResponse response;
        response.set_requestid(request.requestid());
        response.set_accepted(false);
        return response;
    }

    StreamPartHandshakeResponse acceptHandshakeWithInterleaving(
        const StreamPartHandshakeRequest& request,
        const PeerDescriptor& requester) {
        std::vector<DhtAddress> exclude;
        for (const auto& id : request.neighbornodeids()) {
            exclude.push_back(
                Identifiers::getDhtAddressFromRaw(DhtAddressRaw{id}));
        }
        for (const auto& id : this->options.ongoingInterleaves) {
            exclude.push_back(id);
        }
        exclude.push_back(Identifiers::getNodeIdFromPeerDescriptor(requester));
        if (request.has_interleavenodeid()) {
            exclude.push_back(
                Identifiers::getDhtAddressFromRaw(
                    DhtAddressRaw{request.interleavenodeid()}));
        }
        const auto last = this->options.neighbors.getLast(exclude);
        std::optional<PeerDescriptor> lastPeerDescriptor;
        if (last.has_value()) {
            lastPeerDescriptor = last.value()->getPeerDescriptor();
            const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
                lastPeerDescriptor.value());
            const auto remote =
                this->options.createRpcRemote(lastPeerDescriptor.value());
            this->options.ongoingInterleaves.insert(nodeId);
            // TS runs this with then/catch instead of setImmediate to
            // avoid changes in state; here it is a bounded scope task.
            this->interleaveScope.add(
                streamr::utils::co_withExecutor(
                    &this->interleaveExecutor,
                    folly::coro::co_invoke(
                        [this,
                         remote,
                         nodeId,
                         lastDescriptor = lastPeerDescriptor.value(),
                         requester]() -> folly::coro::Task<void> {
                            try {
                                const auto response =
                                    co_await remote->interleaveRequest(
                                        requester);
                                // If accepted, remove the handed-over node from
                                // the target neighbors; otherwise keep it.
                                if (response.accepted()) {
                                    this->options.neighbors.remove(
                                        Identifiers::
                                            getNodeIdFromPeerDescriptor(
                                                lastDescriptor));
                                }
                            } catch (...) {
                                // no-op: interleaveRequest cannot reject
                            }
                            this->options.ongoingInterleaves.erase(nodeId);
                        })));
        }
        this->options.neighbors.add(
            this->options.createContentDeliveryRpcRemote(requester));
        StreamPartHandshakeResponse response;
        response.set_requestid(request.requestid());
        response.set_accepted(true);
        if (lastPeerDescriptor.has_value()) {
            *response.mutable_interleavetargetdescriptor() =
                lastPeerDescriptor.value();
        }
        return response;
    }
};

} // namespace streamr::trackerlessnetwork::neighbordiscovery
