// Module streamr.dht.ExternalApiRpcRemote
// Ported from packages/dht/src/dht/ExternalApiRpcRemote.ts (v103.8.0-rc.3).
// The client used to run recursive operations (fetch/store/find-closest) on a
// remote node "via peer" — i.e. through a connected entry point while the
// local node is still joining. Every call swallows its own error and returns
// an empty result, matching the TS try/catch.
module;

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include <coroutine> // IWYU pragma: keep

export module streamr.dht.ExternalApiRpcRemote;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.logger.SLogger;
import streamr.dht.DhtRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RpcRemote;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht {

using ::dht::DataEntry;
using ::dht::ExternalFetchDataRequest;
using ::dht::ExternalFindClosestNodesRequest;
using ::dht::ExternalStoreDataRequest;
using ::dht::PeerDescriptor;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;

using ExternalApiRpcClient = ::dht::ExternalApiRpcClient<DhtCallContext>;

class ExternalApiRpcRemote : public RpcRemote<ExternalApiRpcClient> {
private:
    static constexpr std::chrono::milliseconds defaultTimeout{10000};

public:
    ExternalApiRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        ExternalApiRpcClient client)
        : RpcRemote<ExternalApiRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              std::move(client)) {}

    folly::coro::Task<std::vector<DataEntry>> externalFetchData(
        const DhtAddress& key) {
        ExternalFetchDataRequest request;
        request.set_key(Identifiers::getRawFromDhtAddress(key));
        auto options = this->formDhtRpcOptions();
        try {
            const auto response = co_await this->getClient().externalFetchData(
                std::move(request), std::move(options), defaultTimeout);
            std::vector<DataEntry> entries;
            entries.reserve(static_cast<size_t>(response.entries_size()));
            for (const auto& entry : response.entries()) {
                entries.push_back(entry);
            }
            co_return entries;
        } catch (const std::exception& err) {
            SLogger::debug(
                "externalFetchData failed: " + std::string(err.what()));
            co_return std::vector<DataEntry>{};
        }
    }

    folly::coro::Task<std::vector<PeerDescriptor>> storeData(
        const DhtAddress& key, const ::google::protobuf::Any& data) {
        ExternalStoreDataRequest request;
        request.set_key(Identifiers::getRawFromDhtAddress(key));
        *request.mutable_data() = data;
        auto options = this->formDhtRpcOptions();
        try {
            const auto response = co_await this->getClient().externalStoreData(
                std::move(request), std::move(options), defaultTimeout);
            std::vector<PeerDescriptor> storers;
            storers.reserve(static_cast<size_t>(response.storers_size()));
            for (const auto& storer : response.storers()) {
                storers.push_back(storer);
            }
            co_return storers;
        } catch (const std::exception& err) {
            SLogger::debug(
                "externalStoreData failed: " + std::string(err.what()));
            co_return std::vector<PeerDescriptor>{};
        }
    }

    folly::coro::Task<std::vector<PeerDescriptor>> externalFindClosestNode(
        const DhtAddress& key) {
        ExternalFindClosestNodesRequest request;
        request.set_nodeid(Identifiers::getRawFromDhtAddress(key));
        auto options = this->formDhtRpcOptions();
        try {
            const auto response =
                co_await this->getClient().externalFindClosestNodes(
                    std::move(request), std::move(options), defaultTimeout);
            std::vector<PeerDescriptor> closestNodes;
            closestNodes.reserve(
                static_cast<size_t>(response.closestnodes_size()));
            for (const auto& node : response.closestnodes()) {
                closestNodes.push_back(node);
            }
            co_return closestNodes;
        } catch (const std::exception& err) {
            SLogger::debug(
                "externalFindClosestNodes failed: " + std::string(err.what()));
            co_return std::vector<PeerDescriptor>{};
        }
    }
};

} // namespace streamr::dht
