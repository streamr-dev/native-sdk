// Module streamr.trackerlessnetwork.FifoMapWithTTL
// CONSOLIDATED from the former header logic/propagation/FifoMapWithTTL.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module streamr.trackerlessnetwork.FifoMapWithTTL;

import streamr.trackerlessnetwork.RandomAccessQueue;
// The MessageRef ordering operator moved to the
// streamr.trackerlessnetwork.protos module (exported there): as a
// non-exported file-scope function here it had module linkage and was
// unreachable from other modules instantiating the map templates.
import streamr.trackerlessnetwork.protos;

export namespace streamr::trackerlessnetwork::propagation {

template <typename KeyType>
struct FifoMapWithTtlOptions {
    std::chrono::milliseconds ttl;
    size_t maxSize;
    std::optional<std::function<void(KeyType)>> onItemDropped;
    std::optional<std::function<std::chrono::milliseconds()>> timeProvider;
};

template <typename KeyType, typename ValueType>
class FifoMapWithTTL {
private:
    struct Item {
        ValueType value;
        QueueToken dropQueueToken;
        std::chrono::milliseconds expiresAt;
    };

    std::recursive_mutex itemsMutex;
    std::map<KeyType, Item> items;
    RandomAccessQueue<KeyType> dropQueue;
    std::chrono::milliseconds ttl;
    size_t maxSize;
    std::function<void(KeyType)> onItemDropped;
    std::function<std::chrono::milliseconds()> timeProvider;

public:
    explicit FifoMapWithTTL(const FifoMapWithTtlOptions<KeyType>& options) {
        this->ttl = options.ttl;
        this->maxSize = options.maxSize;

        if (options.onItemDropped.has_value()) {
            this->onItemDropped = std::move(options.onItemDropped.value());
        } else {
            this->onItemDropped = [](const KeyType&) {};
        }

        if (options.timeProvider.has_value()) {
            this->timeProvider = std::move(options.timeProvider.value());
        } else {
            this->timeProvider = []() {
                return std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch());
            };
        }
    }

    void set(const KeyType& key, const ValueType& value) {
        std::scoped_lock lock{this->itemsMutex};

        if (this->maxSize == 0) {
            return;
        }
        if (this->items.size() > this->maxSize) {
            throw std::logic_error("maximum size exceeded");
        }

        // delete an existing entry if exists
        this->remove(key);

        // make room for new entry
        if (this->items.size() == this->maxSize) {
            const auto keyToDel = this->dropQueue.pop();
            if (!keyToDel.has_value()) {
                throw std::logic_error(
                    "assertion error: queue empty but still have items");
            }
            this->items.erase(keyToDel.value());
            this->onItemDropped(keyToDel.value());
        }

        // add entry

        auto token = this->dropQueue.push(key);
        this->items.emplace(
            key,
            Item{
                .value = value,
                .dropQueueToken = token,
                .expiresAt = this->timeProvider() + this->ttl});
    }

    std::optional<ValueType> get(const KeyType& key) {
        std::scoped_lock lock{this->itemsMutex};

        const auto it = this->items.find(key);
        if (it == this->items.end()) {
            return std::nullopt;
        }
        if (it->second.expiresAt <= this->timeProvider()) {
            this->remove(key);
            return std::nullopt;
        }
        return it->second.value;
    }

    void remove(const KeyType& key) {
        std::scoped_lock lock{this->itemsMutex};

        const auto it = this->items.find(key);
        if (it == this->items.end()) {
            return;
        }
        this->items.erase(it);
        this->dropQueue.remove(it->second.dropQueueToken);
        this->onItemDropped(key);
    }

    std::vector<ValueType> values() {
        std::scoped_lock lock{this->itemsMutex};

        const auto keys =
            this->items | std::views::keys | std::ranges::to<std::vector>();
        std::vector<ValueType> values;
        for (const auto& key : keys) {
            const auto value = this->get(key);
            if (value.has_value()) {
                values.push_back(value.value());
            }
        }
        return values;
    }
};

} // namespace streamr::trackerlessnetwork::propagation
