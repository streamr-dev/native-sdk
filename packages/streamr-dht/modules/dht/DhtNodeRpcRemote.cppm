// Module streamr.dht.DhtNodeRpcRemote
// Ported from packages/dht/src/dht/DhtNodeRpcRemote.ts (v103.8.0-rc.3).
// The client-side wrapper for a peer's DhtNodeRpc service. Also serves as
// the k-bucket contact for that peer (getId()/getVectorClock()).
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <string>

export module streamr.dht.DhtNodeRpcRemote;

import streamr.utils.CoroutineHelper;
import streamr.utils.Uuid;
import streamr.logger.SLogger;
import streamr.dht.DhtRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.ringIdentifiers;
import streamr.dht.RpcRemote;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::Uuid;

export namespace streamr::dht {

using ::dht::ClosestPeersRequest;
using ::dht::ClosestRingPeersRequest;
using ::dht::LeaveNotice;
using ::dht::PeerDescriptor;
using ::dht::PingRequest;
using streamr::dht::contact::RingIdRaw;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;

using DhtNodeRpcClient = ::dht::DhtNodeRpcClient<DhtCallContext>;

// The PeerDescriptor form of a ring query result (the wire response's
// left/right peer lists), distinct from RingContactList's contact-typed
// RingContacts.
struct ClosestRingPeerDescriptors {
    std::vector<PeerDescriptor> left;
    std::vector<PeerDescriptor> right;
};

class DhtNodeRpcRemote : public RpcRemote<DhtNodeRpcClient> {
private:
    inline static int64_t counter =
        0; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    DhtAddressRaw id;
    int64_t vectorClock;
    ServiceID serviceId;

public:
    DhtNodeRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        ServiceID serviceId,
        DhtNodeRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<DhtNodeRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              std::move(client),
              timeout),
          id(DhtAddressRaw{this->getPeerDescriptor().nodeid()}),
          vectorClock(counter++),
          serviceId(std::move(serviceId)) {}

    // Virtual (like ping() below) so tests can substitute a subclass; the
    // RpcRemote base is not polymorphic, so this introduces the vtable.
    virtual ~DhtNodeRpcRemote() = default;

    DhtNodeRpcRemote(const DhtNodeRpcRemote&) = default;
    DhtNodeRpcRemote& operator=(const DhtNodeRpcRemote&) = delete;
    DhtNodeRpcRemote(DhtNodeRpcRemote&&) = delete;
    DhtNodeRpcRemote& operator=(DhtNodeRpcRemote&&) = delete;

    // KBucketContact interface (see streamr.dht.KBucket).
    [[nodiscard]] DhtAddressRaw getId() const { return this->id; }
    [[nodiscard]] int64_t getVectorClock() const { return this->vectorClock; }

    [[nodiscard]] DhtAddress getNodeId() const {
        return Identifiers::getNodeIdFromPeerDescriptor(
            this->getPeerDescriptor());
    }

    // Virtual (like ping()) so tests can substitute a deterministic result.
    virtual folly::coro::Task<std::vector<PeerDescriptor>> getClosestPeers(
        const DhtAddress& nodeId) {
        ClosestPeersRequest request;
        request.set_nodeid(Identifiers::getRawFromDhtAddress(nodeId));
        request.set_requestid(Uuid::v4());
        auto options = this->formDhtRpcOptions();
        auto timeout = this->getTimeout();
        const auto response = co_await this->getClient().getClosestPeers(
            std::move(request), std::move(options), timeout);
        std::vector<PeerDescriptor> peers;
        peers.reserve(static_cast<size_t>(response.peers_size()));
        for (const auto& peer : response.peers()) {
            peers.push_back(peer);
        }
        co_return peers;
    }

    // Virtual (like ping()) so tests can substitute a deterministic result.
    virtual folly::coro::Task<ClosestRingPeerDescriptors> getClosestRingPeers(
        const RingIdRaw& ringIdRaw) {
        ClosestRingPeersRequest request;
        request.set_ringid(ringIdRaw);
        request.set_requestid(Uuid::v4());
        auto options = this->formDhtRpcOptions();
        auto timeout = this->getTimeout();
        const auto response = co_await this->getClient().getClosestRingPeers(
            std::move(request), std::move(options), timeout);
        ClosestRingPeerDescriptors result;
        result.left.reserve(static_cast<size_t>(response.leftpeers_size()));
        for (const auto& peer : response.leftpeers()) {
            result.left.push_back(peer);
        }
        result.right.reserve(static_cast<size_t>(response.rightpeers_size()));
        for (const auto& peer : response.rightpeers()) {
            result.right.push_back(peer);
        }
        co_return result;
    }

    // virtual so tests can substitute a deterministic ping result.
    virtual folly::coro::Task<bool> ping() {
        PingRequest request;
        request.set_requestid(Uuid::v4());
        const std::string requestId = request.requestid();
        auto options = this->formDhtRpcOptions();
        auto timeout = this->getTimeout();
        try {
            const auto pong = co_await this->getClient().ping(
                std::move(request), std::move(options), timeout);
            if (pong.requestid() == requestId) {
                co_return true;
            }
        } catch (const std::exception& err) {
            SLogger::trace(
                "ping failed to " + this->getNodeId() + " " +
                std::string(err.what()));
        }
        co_return false;
    }

    void leaveNotice() {
        LeaveNotice request;
        auto options = this->formDhtRpcOptions();
        auto timeout = this->getTimeout();
        try {
            streamr::utils::blockingWait(this->getClient().leaveNotice(
                std::move(request), std::move(options), timeout));
        } catch (const std::exception& err) {
            SLogger::trace(
                "Failed to send leaveNotice " + std::string(err.what()));
        }
    }
};

} // namespace streamr::dht
