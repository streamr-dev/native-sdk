// Module streamr.dht.LocalDataStore
// Ported from packages/dht/src/dht/store/LocalDataStore.ts
// (v103.8.0-rc.3).
//
// Pulled forward into phase A5 (the rest of the store cluster lands in A6):
// RecursiveOperationManager reads it to answer FIND queries and the A5 unit
// test constructs a real one. Each node can store one value per (data key,
// creator) pair; entries expire after min(entry.ttl, maxTtl).
module;

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.LocalDataStore;

import streamr.utils.MapWithTtl;
import streamr.dht.Identifiers;

export namespace streamr::dht::store {

using ::dht::DataEntry;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::utils::MapWithTtl;

class LocalDataStore {
private:
    uint32_t maxTtl;
    // The outer key is the data key, the inner key is the creator's node id.
    std::map<DhtAddress, MapWithTtl<DhtAddress, DataEntry>> store;

    template <typename Timestamp>
    [[nodiscard]] static int64_t toMillis(const Timestamp& timestamp) {
        constexpr int64_t millisPerSecond = 1000;
        constexpr int64_t nanosPerMilli = 1000000;
        return (timestamp.seconds() * millisPerSecond) +
            (timestamp.nanos() / nanosPerMilli);
    }

public:
    explicit LocalDataStore(uint32_t maxTtl) : maxTtl(maxTtl) {}

    bool storeEntry(const DataEntry& dataEntry) {
        const DhtAddress key =
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{dataEntry.key()});
        const DhtAddress creatorNodeId = Identifiers::getDhtAddressFromRaw(
            DhtAddressRaw{dataEntry.creator()});
        if (!this->store.contains(key)) {
            const uint32_t maxTtlCapture = this->maxTtl;
            this->store.emplace(
                key,
                MapWithTtl<DhtAddress, DataEntry>(
                    [maxTtlCapture](const DataEntry& entry) {
                        return std::chrono::milliseconds(
                            std::min(entry.ttl(), maxTtlCapture));
                    }));
        }
        auto& inner = this->store.at(key);
        if (inner.has(creatorNodeId)) {
            const int64_t storedMillis = toMillis(dataEntry.createdat());
            const int64_t oldStoredMillis =
                toMillis(inner.get(creatorNodeId)->createdat());
            // Do nothing if the local entry is newer than the replicated one.
            if (oldStoredMillis >= storedMillis) {
                return false;
            }
        }
        inner.set(creatorNodeId, dataEntry);
        return true;
    }

    bool markAsDeleted(const DhtAddress& key, const DhtAddress& creator) {
        const auto it = this->store.find(key);
        if (it == this->store.end() || !it->second.has(creator)) {
            return false;
        }
        it->second.get(creator)->set_deleted(true);
        return true;
    }

    [[nodiscard]] std::vector<DataEntry> values(
        const std::optional<DhtAddress>& key = std::nullopt) {
        std::vector<DataEntry> result;
        if (key.has_value()) {
            const auto it = this->store.find(key.value());
            if (it != this->store.end()) {
                auto entries = it->second.values();
                result.insert(result.end(), entries.begin(), entries.end());
            }
        } else {
            for (auto& [dataKey, map] : this->store) {
                auto entries = map.values();
                result.insert(result.end(), entries.begin(), entries.end());
            }
        }
        return result;
    }

    [[nodiscard]] std::vector<DhtAddress> keys() {
        std::vector<DhtAddress> result;
        result.reserve(this->store.size());
        for (const auto& [key, map] : this->store) {
            result.push_back(key);
        }
        return result;
    }

    void setAllEntriesAsStale(const DhtAddress& key) {
        const auto it = this->store.find(key);
        if (it != this->store.end()) {
            it->second.forEach(
                [](DataEntry& value, const DhtAddress& /*creator*/) {
                    value.set_stale(true);
                });
        }
    }

    void deleteEntry(const DhtAddress& key, const DhtAddress& creator) {
        const auto it = this->store.find(key);
        if (it != this->store.end() && it->second.get(creator) != nullptr) {
            it->second.remove(creator);
            if (it->second.size() == 0) {
                this->store.erase(it);
            }
        }
    }

    void clear() {
        for (auto& [key, map] : this->store) {
            map.clear();
        }
        this->store.clear();
    }
};

} // namespace streamr::dht::store
