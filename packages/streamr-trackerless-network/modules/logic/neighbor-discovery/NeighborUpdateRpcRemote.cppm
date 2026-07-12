// Module streamr.trackerlessnetwork.NeighborUpdateRpcRemote
// Ported from packages/trackerless-network/src/content-delivery-layer/
// neighbor-discovery/NeighborUpdateRpcRemote.ts (v103.8.0-rc.3): the
// client side of periodic neighbor-list exchange. Errors are swallowed
// and reported as removeMe=true, matching the TS behavior.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module streamr.trackerlessnetwork.NeighborUpdateRpcRemote;

import streamr.trackerlessnetwork.protos;

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RpcRemote;
import streamr.dht.protos;
import streamr.logger.SLogger;
import streamr.utils.StreamPartID;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using streamr::dht::Identifiers;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::logger::SLogger;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork::neighbordiscovery {

using ::dht::PeerDescriptor;
using NeighborUpdateRpcClient =
    streamr::protorpc::NeighborUpdateRpcClient<DhtCallContext>;

struct UpdateNeighborsResponse {
    std::vector<PeerDescriptor> peerDescriptors;
    bool removeMe = false;
};

class NeighborUpdateRpcRemote : public RpcRemote<NeighborUpdateRpcClient> {
public:
    NeighborUpdateRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        NeighborUpdateRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<NeighborUpdateRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              client,
              timeout) {}

    folly::coro::Task<UpdateNeighborsResponse> updateNeighbors(
        StreamPartID streamPartId, std::vector<PeerDescriptor> neighbors) {
        NeighborUpdate request;
        request.set_streampartid(streamPartId);
        for (auto& neighbor : neighbors) {
            *request.add_neighbordescriptors() = std::move(neighbor);
        }
        request.set_removeme(false);
        auto options = this->formDhtRpcOptions({});
        try {
            const auto response = co_await this->getClient().neighborUpdate(
                std::move(request), std::move(options));
            UpdateNeighborsResponse result;
            result.peerDescriptors.assign(
                response.neighbordescriptors().begin(),
                response.neighbordescriptors().end());
            result.removeMe = response.removeme();
            co_return result;
        } catch (const std::exception& err) {
            SLogger::debug(
                "updateNeighbors to " +
                Identifiers::getNodeIdFromPeerDescriptor(
                    this->getPeerDescriptor()) +
                " failed: " + std::string(err.what()));
            co_return UpdateNeighborsResponse{.removeMe = true};
        }
    }
};

} // namespace streamr::trackerlessnetwork::neighbordiscovery
