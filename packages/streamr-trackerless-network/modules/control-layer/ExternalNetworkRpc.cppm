// Module streamr.trackerlessnetwork.ExternalNetworkRpc
// Ported from packages/trackerless-network/src/control-layer/
// ExternalNetworkRpc.ts (v103.8.0-rc.3): a ListeningRpcCommunicator on
// the dedicated external-network service that applications use to
// register their own RPC methods and create clients for them. The TS
// class-object parameters (request/response IMessageType, client
// class) are template parameters here.
module;

#include <string>
#include <utility>

export module streamr.trackerlessnetwork.ExternalNetworkRpc;

import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.Transport;

using streamr::dht::ServiceID;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::Transport;

export namespace streamr::trackerlessnetwork::controllayer {

// TS SERVICE_ID.
inline constexpr auto externalNetworkServiceId = "external-network-service";

class ExternalNetworkRpc {
private:
    ListeningRpcCommunicator rpcCommunicator;

public:
    explicit ExternalNetworkRpc(Transport& transport)
        : rpcCommunicator(ServiceID{externalNetworkServiceId}, transport) {}

    template <typename RequestType, typename ResponseType, typename F>
    void registerRpcMethod(const std::string& name, F&& fn) {
        this->rpcCommunicator.registerRpcMethod<RequestType, ResponseType>(
            name, std::forward<F>(fn));
    }

    template <typename RequestType, typename F>
    void registerRpcNotification(const std::string& name, F&& fn) {
        this->rpcCommunicator.registerRpcNotification<RequestType>(
            name, std::forward<F>(fn));
    }

    // TS createRpcClient(clientClass); the generated client type (e.g.
    // HandshakeRpcClient<DhtCallContext>) is the template argument.
    template <typename ClientType>
    [[nodiscard]] ClientType createRpcClient() {
        return ClientType{this->rpcCommunicator};
    }

    [[nodiscard]] ListeningRpcCommunicator& getRpcCommunicator() {
        return this->rpcCommunicator;
    }

    void destroy() { this->rpcCommunicator.destroy(); }
};

} // namespace streamr::trackerlessnetwork::controllayer
