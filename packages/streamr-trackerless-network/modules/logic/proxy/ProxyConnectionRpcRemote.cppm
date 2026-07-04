// Module streamr.trackerlessnetwork.ProxyConnectionRpcRemote
// CONSOLIDATED from the former header logic/proxy/ProxyConnectionRpcRemote.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <folly/experimental/coro/Task.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.client.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.ProxyConnectionRpcRemote;

import streamr.dht.DhtCallContext;
import streamr.dht.RpcRemote;
import streamr.dht.protos;
import streamr.logger;
import streamr.utils;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified because relative namespace names resolve
// differently at file scope than inside the package namespace.
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::logger::SLogger;
using streamr::utils::BinaryUtils;
using streamr::utils::EthereumAddress;
export namespace streamr::trackerlessnetwork::proxy {

using ProxyConnectionRpcClient =
    streamr::protorpc::ProxyConnectionRpcClient<DhtCallContext>;
using ::dht::PeerDescriptor;

class ProxyConnectionRpcRemote : public RpcRemote<ProxyConnectionRpcClient> {
public:
    ProxyConnectionRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        ProxyConnectionRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<ProxyConnectionRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              client,
              timeout) {}
    folly::coro::Task<bool> requestConnection(
        ProxyDirection direction, const EthereumAddress& userId) {
        ProxyConnectionRequest request;
        request.set_direction(direction);
        request.set_userid(BinaryUtils::hexToBinaryString(userId));

        auto options = this->formDhtRpcOptions();

        auto result = co_await this->getClient().requestConnection(
            std::move(request),
            std::move(options),
            RpcRemote::existingConnectionTimeout);

        co_return result.accepted();
    }
};

} // namespace streamr::trackerlessnetwork::proxy
