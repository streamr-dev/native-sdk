// Module streamr.dht.ExternalApiRpcLocal
// Ported from packages/dht/src/dht/ExternalApiRpcLocal.ts (v103.8.0-rc.3).
// The server side of the ExternalApi service: it lets a peer run a recursive
// fetch/store/find-closest on this node's behalf (used while that peer is
// still joining and can only reach the DHT through a connected entry point).
//
// The handlers are coroutines, not implementations of the generated
// (synchronous) ExternalApiRpc interface: each runs a recursive DHT operation
// that sends messages and awaits their responses, so it must SUSPEND rather
// than block — DhtNode registers them with registerRpcMethodAsync.
module;

#include <functional>
#include <vector>

#include <coroutine> // IWYU pragma: keep

export module streamr.dht.ExternalApiRpcLocal;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RecursiveOperationSession;

export namespace streamr::dht {

using ::dht::ExternalFetchDataRequest;
using ::dht::ExternalFetchDataResponse;
using ::dht::ExternalFindClosestNodesRequest;
using ::dht::ExternalFindClosestNodesResponse;
using ::dht::ExternalStoreDataRequest;
using ::dht::ExternalStoreDataResponse;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperation;
using streamr::dht::Identifiers;
using streamr::dht::recursiveoperation::RecursiveOperationResult;
using streamr::dht::rpcprotocol::DhtCallContext;

struct ExternalApiRpcLocalOptions {
    std::function<folly::coro::Task<RecursiveOperationResult>(
        const DhtAddress& targetId,
        RecursiveOperation operation,
        const DhtAddress& excludedPeer)>
        executeRecursiveOperation;
    std::function<folly::coro::Task<std::vector<PeerDescriptor>>(
        const DhtAddress& key,
        const ::google::protobuf::Any& data,
        const DhtAddress& creator)>
        storeDataToDht;
};

class ExternalApiRpcLocal {
private:
    ExternalApiRpcLocalOptions options;

public:
    explicit ExternalApiRpcLocal(ExternalApiRpcLocalOptions options)
        : options(std::move(options)) {}

    folly::coro::Task<ExternalFetchDataResponse> externalFetchData(
        ExternalFetchDataRequest request, DhtCallContext context) {
        const auto sender = context.incomingSourceDescriptor.value();
        const auto result = co_await this->options.executeRecursiveOperation(
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{request.key()}),
            RecursiveOperation::FETCH_DATA,
            Identifiers::getNodeIdFromPeerDescriptor(sender));
        ExternalFetchDataResponse response;
        for (const auto& entry : result.dataEntries) {
            *response.add_entries() = entry;
        }
        co_return response;
    }

    folly::coro::Task<ExternalStoreDataResponse> externalStoreData(
        ExternalStoreDataRequest request, DhtCallContext context) {
        const auto sender = context.incomingSourceDescriptor.value();
        const auto storers = co_await this->options.storeDataToDht(
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{request.key()}),
            request.data(),
            Identifiers::getNodeIdFromPeerDescriptor(sender));
        ExternalStoreDataResponse response;
        for (const auto& storer : storers) {
            *response.add_storers() = storer;
        }
        co_return response;
    }

    folly::coro::Task<ExternalFindClosestNodesResponse>
    externalFindClosestNodes(
        ExternalFindClosestNodesRequest request, DhtCallContext context) {
        const auto sender = context.incomingSourceDescriptor.value();
        const auto result = co_await this->options.executeRecursiveOperation(
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{request.nodeid()}),
            RecursiveOperation::FIND_CLOSEST_NODES,
            Identifiers::getNodeIdFromPeerDescriptor(sender));
        ExternalFindClosestNodesResponse response;
        for (const auto& node : result.closestNodes) {
            *response.add_closestnodes() = node;
        }
        co_return response;
    }
};

} // namespace streamr::dht
