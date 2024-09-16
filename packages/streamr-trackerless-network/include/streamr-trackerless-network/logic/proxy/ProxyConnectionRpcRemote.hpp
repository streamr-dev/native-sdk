#ifndef STREAMR_TRACKERLESS_NETWORK_PROXY_PROXYCONNECTIONRPCREMOTE_HPP
#define STREAMR_TRACKERLESS_NETWORK_PROXY_PROXYCONNECTIONRPCREMOTE_HPP

#include <folly/coro/Task.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.client.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "streamr-dht/dht/contact/RpcRemote.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/BinaryUtils.hpp"
#include "streamr-utils/EthereumAddress.hpp"

namespace streamr::trackerlessnetwork::proxy {

using streamr::dht::rpcprotocol::DhtCallContext;
using ProxyConnectionRpcClient =
    streamr::protorpc::ProxyConnectionRpcClient<DhtCallContext>;
using ::dht::PeerDescriptor;
using streamr::dht::contact::RpcRemote;
using streamr::logger::SLogger;
using streamr::utils::BinaryUtils;
using streamr::utils::EthereumAddress;

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

        try {
            auto result = co_await this->getClient().requestConnection(
                std::move(request),
                std::move(options),
                RpcRemote::existingConnectionTimeout);
            co_return result.accepted();
        } catch (const std::exception& e) {
            SLogger::debug(
                "ProxyConnectionRequest failed with error " +
                std::string(e.what()));
            co_return false;
        }
    }
};

} // namespace streamr::trackerlessnetwork::proxy

#endif // STREAMR_TRACKERLESS_NETWORK_PROXY_PROXYCONNECTIONRPCREMOTE_HPP