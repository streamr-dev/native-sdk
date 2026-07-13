// Module streamr.trackerlessnetwork.NetworkNode
// Ported from packages/trackerless-network/src/NetworkNode.ts
// (v103.8.0-rc.3): the public client-facing API over a NetworkStack.
//
// Adaptations: addMessageListener returns a HandlerToken and
// removeMessageListener takes it (the C++ EventEmitter unsubscribes by
// token, not listener identity). getMetricsContext/getDiagnosticInfo
// are not ported (consistent with earlier phases). The TS generic
// registerExternalNetworkRpcMethod/createExternalRpcClient class-object
// parameters are template parameters.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module streamr.trackerlessnetwork.NetworkNode;

import streamr.utils.CoroutineHelper;
import streamr.utils.EthereumAddress;
import streamr.utils.StreamPartID;
import streamr.trackerlessnetwork.protos;
import streamr.trackerlessnetwork.ContentDeliveryManager;
import streamr.trackerlessnetwork.ExternalNetworkRpc;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.eventemitter.EventEmitter;

using streamr::dht::DhtAddress;
using streamr::eventemitter::HandlerToken;
using streamr::utils::EthereumAddress;
using streamr::utils::StreamPartID;

export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using streamr::trackerlessnetwork::controllayer::ExternalNetworkRpc;

/**
 * Convenience wrapper for building client-facing functionality (TS
 * comment: used by client).
 */
class NetworkNode {
private:
    std::shared_ptr<NetworkStack> stack;
    std::unique_ptr<ExternalNetworkRpc> externalNetworkRpc;
    bool stopped = false;

public:
    explicit NetworkNode(std::shared_ptr<NetworkStack> stack)
        : stack(std::move(stack)) {}

    folly::coro::Task<void> start(bool doJoin = true) {
        co_await this->stack->start(doJoin);
        this->externalNetworkRpc =
            std::make_unique<ExternalNetworkRpc>(*this->stack->getTransport());
    }

    folly::coro::Task<bool> inspect(
        PeerDescriptor node, StreamPartID streamPartId) {
        co_return co_await this->stack->getContentDeliveryManager().inspect(
            std::move(node), std::move(streamPartId));
    }

    folly::coro::Task<void> broadcast(const StreamMessage& msg) {
        co_await this->stack->broadcast(msg);
    }

    folly::coro::Task<void> join(
        StreamPartID streamPartId,
        std::optional<NeighborRequirement> neighborRequirement = std::nullopt) {
        co_await this->stack->joinStreamPart(
            std::move(streamPartId), neighborRequirement);
    }

    folly::coro::Task<void> setProxies(
        StreamPartID streamPartId,
        std::vector<PeerDescriptor> nodes,
        std::optional<ProxyDirection> direction,
        const EthereumAddress& userId,
        std::optional<size_t> connectionCount = std::nullopt) {
        co_await this->stack->getContentDeliveryManager().setProxies(
            std::move(streamPartId),
            std::move(nodes),
            direction,
            userId,
            connectionCount);
    }

    [[nodiscard]] bool isProxiedStreamPart(
        const StreamPartID& streamPartId) const {
        return this->stack->getContentDeliveryManager().isProxiedStreamPart(
            streamPartId);
    }

    // The C++ EventEmitter unsubscribes by token: keep the returned
    // token and pass it to removeMessageListener (TS removes by
    // listener identity).
    HandlerToken addMessageListener(
        const std::function<void(const StreamMessage&)>& listener) {
        return this->stack->getContentDeliveryManager()
            .on<contentdeliverymanagerevents::NewMessage>(listener);
    }

    void removeMessageListener(HandlerToken token) {
        this->stack->getContentDeliveryManager()
            .off<contentdeliverymanagerevents::NewMessage>(token);
    }

    void setStreamPartEntryPoints(
        const StreamPartID& streamPartId,
        std::vector<PeerDescriptor> contactPeerDescriptors) {
        this->stack->getContentDeliveryManager().setStreamPartEntryPoints(
            streamPartId, std::move(contactPeerDescriptors));
    }

    folly::coro::Task<void> leave(StreamPartID streamPartId) {
        if (this->stopped) {
            co_return;
        }
        co_await this->stack->getContentDeliveryManager().leaveStreamPart(
            std::move(streamPartId));
    }

    [[nodiscard]] std::vector<DhtAddress> getNeighbors(
        const StreamPartID& streamPartId) const {
        return this->stack->getContentDeliveryManager().getNeighbors(
            streamPartId);
    }

    [[nodiscard]] bool hasStreamPart(const StreamPartID& streamPartId) const {
        return this->stack->getContentDeliveryManager().hasStreamPart(
            streamPartId);
    }

    folly::coro::Task<void> stop() {
        this->stopped = true;
        if (this->externalNetworkRpc) {
            this->externalNetworkRpc->destroy();
        }
        co_await this->stack->stop();
    }

    [[nodiscard]] PeerDescriptor getPeerDescriptor() {
        return this->stack->getControlLayerNode().getLocalPeerDescriptor();
    }

    [[nodiscard]] DhtAddress getNodeId() const {
        return this->stack->getContentDeliveryManager().getNodeId();
    }

    [[nodiscard]] const NetworkOptions& getOptions() const {
        return this->stack->getOptions();
    }

    [[nodiscard]] std::vector<StreamPartID> getStreamParts() const {
        return this->stack->getContentDeliveryManager().getStreamParts();
    }

    folly::coro::Task<NodeInfoResponse> fetchNodeInfo(PeerDescriptor node) {
        co_return co_await this->stack->fetchNodeInfo(std::move(node));
    }

    template <typename RequestType, typename ResponseType, typename F>
    void registerExternalNetworkRpcMethod(const std::string& name, F&& fn) {
        this->externalNetworkRpc->registerRpcMethod<RequestType, ResponseType>(
            name, std::forward<F>(fn));
    }

    template <typename ClientType>
    [[nodiscard]] ClientType createExternalRpcClient() {
        return this->externalNetworkRpc->createRpcClient<ClientType>();
    }

    // TS exposes the stack as a readonly field; the proxy end-to-end
    // tests reach through it.
    [[nodiscard]] NetworkStack& getStack() { return *this->stack; }
};

inline std::shared_ptr<NetworkNode> createNetworkNode(NetworkOptions options) {
    return std::make_shared<NetworkNode>(
        std::make_shared<NetworkStack>(std::move(options)));
}

} // namespace streamr::trackerlessnetwork
