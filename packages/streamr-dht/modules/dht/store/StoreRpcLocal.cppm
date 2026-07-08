// Module streamr.dht.StoreRpcLocal
// Ported from packages/dht/src/dht/store/StoreRpcLocal.ts (v103.8.0-rc.3).
// The server side of the StoreRpc service: storeData writes an entry (stale
// unless this node is a storer of the key); replicateData writes a
// replicated entry and, if newly stored, fans it out to the neighbouring
// storers.
//
// TS defers each fan-out replication with setImmediate + executeSafePromise
// (fire and forget). Here replicateDataToContact is a synchronous void
// callback that the manager wires to kick off the async replication itself,
// so the local side calls it directly.
module;
#include <new>


export module streamr.dht.StoreRpcLocal;

import std;

import streamr.dht.protos;

import streamr.logger.SLogger;
import streamr.dht.DhtRpcServer;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.LocalDataStore;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht::store {

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using ::dht::ReplicateDataRequest;
using ::dht::StoreDataRequest;
using ::dht::StoreDataResponse;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;

using StoreRpc = ::dht::StoreRpc<DhtCallContext>;

// Timestamp for "now" (protobuf has no Timestamp::now() in C++).
inline ::google::protobuf::Timestamp nowTimestamp() {
    const auto sinceEpoch = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(sinceEpoch);
    const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(
        sinceEpoch - seconds);
    ::google::protobuf::Timestamp timestamp;
    timestamp.set_seconds(seconds.count());
    timestamp.set_nanos(static_cast<std::int32_t>(nanos.count()));
    return timestamp;
}

struct StoreRpcLocalOptions {
    LocalDataStore& localDataStore;
    PeerDescriptor localPeerDescriptor;
    std::function<void(const DataEntry&, const PeerDescriptor&)>
        replicateDataToContact;
    std::function<std::vector<PeerDescriptor>(const DhtAddress&)> getStorers;
};

class StoreRpcLocal : public StoreRpc {
private:
    StoreRpcLocalOptions options;

    [[nodiscard]] bool isLocalNodeStorer(const DhtAddress& dataKey) {
        const auto storers = this->options.getStorers(dataKey);
        return std::ranges::any_of(storers, [this](const PeerDescriptor& peer) {
            return Identifiers::areEqualPeerDescriptors(
                peer, this->options.localPeerDescriptor);
        });
    }

    void replicateDataToNeighbors(
        const PeerDescriptor& requestor, const DataEntry& dataEntry) {
        const DhtAddress dataKey =
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{dataEntry.key()});
        const auto storers = this->options.getStorers(dataKey);
        if (storers.empty()) {
            return;
        }
        const bool isLocalNodePrimaryStorer =
            Identifiers::areEqualPeerDescriptors(
                storers[0], this->options.localPeerDescriptor);
        // If we are the closest to the data, replicate to all storers;
        // otherwise only to the closest one. Never to the requestor or self.
        const std::vector<PeerDescriptor> candidates = isLocalNodePrimaryStorer
            ? storers
            : std::vector<PeerDescriptor>{storers[0]};
        for (const auto& target : candidates) {
            if (!Identifiers::areEqualPeerDescriptors(target, requestor) &&
                !Identifiers::areEqualPeerDescriptors(
                    target, this->options.localPeerDescriptor)) {
                this->options.replicateDataToContact(dataEntry, target);
            }
        }
    }

public:
    explicit StoreRpcLocal(StoreRpcLocalOptions options)
        : options(std::move(options)) {}

    ~StoreRpcLocal() override = default;

    StoreDataResponse storeData(
        const StoreDataRequest& request,
        const DhtCallContext& /*callContext*/) override {
        SLogger::trace("storeData()");
        const DhtAddress key =
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{request.key()});
        const bool isLocalNodeStorer = this->isLocalNodeStorer(key);
        DataEntry entry;
        entry.set_key(request.key());
        *entry.mutable_data() = request.data();
        entry.set_creator(request.creator());
        *entry.mutable_createdat() = request.createdat();
        *entry.mutable_storedat() = nowTimestamp();
        entry.set_ttl(request.ttl());
        entry.set_stale(!isLocalNodeStorer);
        entry.set_deleted(false);
        this->options.localDataStore.storeEntry(entry);
        if (!isLocalNodeStorer) {
            this->options.localDataStore.setAllEntriesAsStale(key);
        }
        return StoreDataResponse{};
    }

    void replicateData(
        const ReplicateDataRequest& request,
        const DhtCallContext& callContext) override {
        SLogger::trace("server-side replicateData()");
        const DataEntry& dataEntry = request.entry();
        const bool wasStored =
            this->options.localDataStore.storeEntry(dataEntry);
        if (wasStored) {
            this->replicateDataToNeighbors(
                callContext.incomingSourceDescriptor.value(), request.entry());
        }
        const DhtAddress key =
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{dataEntry.key()});
        if (!this->isLocalNodeStorer(key)) {
            this->options.localDataStore.setAllEntriesAsStale(key);
        }
    }
};

} // namespace streamr::dht::store
