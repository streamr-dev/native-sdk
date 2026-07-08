// Module streamr.dht.StoreRpcRemote
// Ported from packages/dht/src/dht/store/StoreRpcRemote.ts
// (v103.8.0-rc.3). The client used to store data on, and replicate data to,
// a peer. storeData/replicateData are virtual so a test can substitute a
// counting mock.
module;
#include <new> // operator new ambiguity under import std (local-type container allocation) — see convert-to-import-std.py



export module streamr.dht.StoreRpcRemote;

import std;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.logger.SLogger;
import streamr.dht.DhtRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.RpcRemote;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht::store {

using ::dht::PeerDescriptor;
using ::dht::ReplicateDataRequest;
using ::dht::StoreDataRequest;
using streamr::dht::Identifiers;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;

using StoreRpcClient = ::dht::StoreRpcClient<DhtCallContext>;

class StoreRpcRemote : public RpcRemote<StoreRpcClient> {
public:
    StoreRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        StoreRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<StoreRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              std::move(client),
              timeout) {}

    virtual ~StoreRpcRemote() = default;
    StoreRpcRemote(const StoreRpcRemote&) = delete;
    StoreRpcRemote& operator=(const StoreRpcRemote&) = delete;
    StoreRpcRemote(StoreRpcRemote&&) = delete;
    StoreRpcRemote& operator=(StoreRpcRemote&&) = delete;

    virtual folly::coro::Task<void> storeData(StoreDataRequest request) {
        auto options = this->formDhtRpcOptions();
        try {
            co_await this->getClient().storeData(
                std::move(request), std::move(options), this->getTimeout());
        } catch (const std::exception& err) {
            const DhtAddress to = Identifiers::getNodeIdFromPeerDescriptor(
                this->getPeerDescriptor());
            const DhtAddress from = Identifiers::getNodeIdFromPeerDescriptor(
                this->getLocalPeerDescriptor());
            throw std::runtime_error(
                "Could not store data to " + to + " from " + from + " " +
                std::string(err.what()));
        }
    }

    virtual folly::coro::Task<void> replicateData(
        ReplicateDataRequest request, bool connect) {
        DhtCallContext context;
        context.connect = connect;
        context.sendIfStopped = true;
        auto options = this->formDhtRpcOptions(context);
        co_await this->getClient().replicateData(
            std::move(request),
            std::move(options),
            RpcRemote<StoreRpcClient>::existingConnectionTimeout);
    }
};

} // namespace streamr::dht::store
