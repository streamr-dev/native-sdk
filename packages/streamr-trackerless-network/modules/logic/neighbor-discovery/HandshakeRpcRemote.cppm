// Module streamr.trackerlessnetwork.HandshakeRpcRemote
// Ported from packages/trackerless-network/src/content-delivery-layer/
// neighbor-discovery/HandshakeRpcRemote.ts (v103.8.0-rc.3): the client
// side of stream-part neighbor handshakes. Both RPCs swallow errors and
// report them as not-accepted, matching the TS behavior.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module streamr.trackerlessnetwork.HandshakeRpcRemote;

import streamr.trackerlessnetwork.protos;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RpcRemote;
import streamr.dht.protos;
import streamr.logger.SLogger;
import streamr.utils.StreamPartID;
import streamr.utils.Uuid;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::logger::SLogger;
using streamr::utils::StreamPartID;
using streamr::utils::Uuid;

export namespace streamr::trackerlessnetwork::neighbordiscovery {

using ::dht::PeerDescriptor;
using HandshakeRpcClient =
    streamr::protorpc::HandshakeRpcClient<DhtCallContext>;

constexpr std::chrono::milliseconds interleaveRequestTimeout{10000};

struct HandshakeResponse {
    bool accepted = false;
    std::optional<PeerDescriptor> interleaveTargetDescriptor;
};

class HandshakeRpcRemote : public RpcRemote<HandshakeRpcClient> {
public:
    HandshakeRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        HandshakeRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<HandshakeRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              client,
              timeout) {}

    folly::coro::Task<HandshakeResponse> handshake(
        StreamPartID streamPartId,
        std::vector<DhtAddress> neighborNodeIds,
        std::optional<DhtAddress> concurrentHandshakeNodeId = std::nullopt,
        std::optional<DhtAddress> interleaveNodeId = std::nullopt) {
        StreamPartHandshakeRequest request;
        request.set_streampartid(streamPartId);
        request.set_requestid(Uuid::v4());
        for (const auto& id : neighborNodeIds) {
            request.add_neighbornodeids(Identifiers::getRawFromDhtAddress(id));
        }
        if (concurrentHandshakeNodeId.has_value()) {
            request.set_concurrenthandshakenodeid(
                Identifiers::getRawFromDhtAddress(
                    concurrentHandshakeNodeId.value()));
        }
        if (interleaveNodeId.has_value()) {
            request.set_interleavenodeid(
                Identifiers::getRawFromDhtAddress(interleaveNodeId.value()));
        }
        auto options = this->formDhtRpcOptions({});
        try {
            const auto response = co_await this->getClient().handshake(
                std::move(request), std::move(options));
            HandshakeResponse result{.accepted = response.accepted()};
            if (response.has_interleavetargetdescriptor()) {
                result.interleaveTargetDescriptor =
                    response.interleavetargetdescriptor();
            }
            co_return result;
        } catch (const std::exception& err) {
            SLogger::debug(
                "handshake to " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->getPeerDescriptor()) +
                " failed: " + std::string(err.what()));
            co_return HandshakeResponse{.accepted = false};
        }
    }

    folly::coro::Task<InterleaveResponse> interleaveRequest(
        PeerDescriptor originatorDescriptor) {
        InterleaveRequest request;
        *request.mutable_interleavetargetdescriptor() =
            std::move(originatorDescriptor);
        DhtCallContext opts;
        opts.connect = false;
        auto options = this->formDhtRpcOptions(opts);
        try {
            co_return co_await this->getClient().interleaveRequest(
                std::move(request),
                std::move(options),
                interleaveRequestTimeout);
        } catch (const std::exception& err) {
            SLogger::debug(
                "interleaveRequest to " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->getPeerDescriptor()) +
                " failed: " + std::string(err.what()));
            InterleaveResponse response;
            response.set_accepted(false);
            co_return response;
        }
    }
};

} // namespace streamr::trackerlessnetwork::neighbordiscovery
