// Module streamr.dht.RoutingTablesCache
// Ported from packages/dht/src/dht/routing/RoutingTablesCache.ts
// (v103.8.0-rc.3).
//
// A cache of per-(targetId, previousId) routing tables. Storing previousId
// matters: it is the minimum-distance bound of the table's contacts.
// Building a table from scratch is O(n log n) in the number of a node's
// connections; adding/removing a single contact is O(log n)/O(1), so
// keeping the hottest tables in memory and updating them on connect/
// disconnect is a large win. TS uses the npm lru-cache (max 1000, 15 s
// TTL); this file ports that with a small internal LRU-with-TTL.
module;


export module streamr.dht.RoutingTablesCache;

import std;

import streamr.dht.Identifiers;
import streamr.dht.RoutingRemoteContact;
import streamr.dht.SortedContactList;

export namespace streamr::dht::routing {

using streamr::dht::DhtAddress;
using streamr::dht::contact::SortedContactList;

// A routing table is a distance-sorted contact list; the cache only ever
// uses the subset of its API listed in the TS `RoutingTable` Pick type
// (getClosestContacts / addContacts / addContact / removeContact / stop /
// getSize).
using RoutingTable = std::shared_ptr<SortedContactList<RoutingRemoteContact>>;

namespace detail {

// Minimal LRU cache with a per-entry time-to-live, matching the semantics
// the routing cache relies on: get() refreshes recency and drops stale
// entries; has() checks presence and staleness without reordering; the
// least-recently-used entry is evicted once the size bound is exceeded.
template <typename Value>
class LruCache {
private:
    struct Entry {
        std::string key;
        Value value;
        std::chrono::steady_clock::time_point insertedAt;
    };

    std::size_t maxSize;
    std::chrono::milliseconds maxAge;
    std::list<Entry> entries; // front = most recently used
    std::unordered_map<std::string, typename std::list<Entry>::iterator> index;

    [[nodiscard]] bool expired(
        const Entry& entry, std::chrono::steady_clock::time_point now) const {
        return (now - entry.insertedAt) > this->maxAge;
    }

public:
    LruCache(std::size_t maxSize, std::chrono::milliseconds maxAge)
        : maxSize(maxSize), maxAge(maxAge) {}

    std::optional<Value> get(const std::string& key) {
        const auto it = this->index.find(key);
        if (it == this->index.end()) {
            return std::nullopt;
        }
        const auto now = std::chrono::steady_clock::now();
        if (this->expired(*it->second, now)) {
            this->entries.erase(it->second);
            this->index.erase(it);
            return std::nullopt;
        }
        this->entries.splice(this->entries.begin(), this->entries, it->second);
        return it->second->value;
    }

    [[nodiscard]] bool has(const std::string& key) {
        const auto it = this->index.find(key);
        if (it == this->index.end()) {
            return false;
        }
        if (this->expired(*it->second, std::chrono::steady_clock::now())) {
            this->entries.erase(it->second);
            this->index.erase(it);
            return false;
        }
        return true;
    }

    void set(const std::string& key, Value value) {
        const auto now = std::chrono::steady_clock::now();
        const auto it = this->index.find(key);
        if (it != this->index.end()) {
            it->second->value = std::move(value);
            it->second->insertedAt = now;
            this->entries.splice(
                this->entries.begin(), this->entries, it->second);
            return;
        }
        this->entries.push_front(Entry{key, std::move(value), now});
        this->index[key] = this->entries.begin();
        if (this->index.size() > this->maxSize) {
            this->index.erase(this->entries.back().key);
            this->entries.pop_back();
        }
    }

    template <typename Fn>
    void forEach(Fn fn) {
        for (auto& entry : this->entries) {
            fn(entry.value);
        }
    }

    void clear() {
        this->entries.clear();
        this->index.clear();
    }
};

} // namespace detail

class RoutingTablesCache {
private:
    static constexpr std::size_t defaultMaxTables = 1000;
    static constexpr std::chrono::milliseconds defaultMaxAge{15000};

    detail::LruCache<RoutingTable> tables{defaultMaxTables, defaultMaxAge};

    static std::string createRoutingTableId(
        const DhtAddress& targetId,
        const std::optional<DhtAddress>& previousId) {
        return std::string(targetId) +
            (previousId.has_value() ? std::string(previousId.value())
                                    : std::string());
    }

public:
    [[nodiscard]] RoutingTable get(
        const DhtAddress& targetId,
        const std::optional<DhtAddress>& previousId = std::nullopt) {
        return this->tables.get(createRoutingTableId(targetId, previousId))
            .value_or(nullptr);
    }

    void set(
        const DhtAddress& targetId,
        const RoutingTable& table,
        const std::optional<DhtAddress>& previousId = std::nullopt) {
        this->tables.set(createRoutingTableId(targetId, previousId), table);
    }

    [[nodiscard]] bool has(
        const DhtAddress& targetId,
        const std::optional<DhtAddress>& previousId = std::nullopt) {
        return this->tables.has(createRoutingTableId(targetId, previousId));
    }

    void onNodeDisconnected(const DhtAddress& nodeId) {
        this->tables.forEach([&nodeId](const RoutingTable& table) {
            table->removeContact(nodeId);
        });
    }

    void onNodeConnected(const std::shared_ptr<RoutingRemoteContact>& remote) {
        this->tables.forEach([&remote](const RoutingTable& table) {
            table->addContact(remote);
        });
    }

    void reset() {
        this->tables.forEach([](const RoutingTable& table) { table->stop(); });
        this->tables.clear();
    }
};

} // namespace streamr::dht::routing
