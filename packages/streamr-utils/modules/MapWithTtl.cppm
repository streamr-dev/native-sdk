// Module streamr.utils.MapWithTtl
// Ported from packages/utils/src/MapWithTtl.ts. A map whose entries expire
// getTtl(value) milliseconds after they are set.
//
// TS removes each entry with its own setTimeout. This C++ port expires
// lazily — an entry past its deadline is dropped the next time the map is
// touched — which keeps the same observable behaviour (get/has/values/
// size/forEach/keys never surface an expired entry) without the lifetime
// hazard of a per-entry timer callback capturing a map that is not owned
// through a shared_ptr.
module;

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <utility>
#include <vector>

export module streamr.utils.MapWithTtl;

export namespace streamr::utils {

template <typename K, typename V>
class MapWithTtl {
private:
    struct ValueWrapper {
        V value;
        std::chrono::steady_clock::time_point expiresAt;
    };

    std::map<K, ValueWrapper> delegate;
    std::function<std::chrono::milliseconds(const V&)> getTtl;

    [[nodiscard]] static bool expired(
        const ValueWrapper& wrapper,
        std::chrono::steady_clock::time_point now) {
        return now >= wrapper.expiresAt;
    }

    void purgeExpired() {
        const auto now = std::chrono::steady_clock::now();
        for (auto it = this->delegate.begin(); it != this->delegate.end();) {
            if (expired(it->second, now)) {
                it = this->delegate.erase(it);
            } else {
                ++it;
            }
        }
    }

public:
    explicit MapWithTtl(
        std::function<std::chrono::milliseconds(const V&)> getTtl)
        : getTtl(std::move(getTtl)) {}

    void set(const K& key, const V& value) {
        const auto now = std::chrono::steady_clock::now();
        this->delegate.insert_or_assign(
            key, ValueWrapper{value, now + this->getTtl(value)});
    }

    // Returns a pointer to the stored value (so callers can mutate it, as
    // the TS callers do), or nullptr if the key is absent or expired.
    [[nodiscard]] V* get(const K& key) {
        const auto it = this->delegate.find(key);
        if (it == this->delegate.end()) {
            return nullptr;
        }
        if (expired(it->second, std::chrono::steady_clock::now())) {
            this->delegate.erase(it);
            return nullptr;
        }
        return &it->second.value;
    }

    [[nodiscard]] bool has(const K& key) {
        const auto it = this->delegate.find(key);
        if (it == this->delegate.end()) {
            return false;
        }
        if (expired(it->second, std::chrono::steady_clock::now())) {
            this->delegate.erase(it);
            return false;
        }
        return true;
    }

    // Named remove() rather than delete() (a C++ keyword).
    void remove(const K& key) { this->delegate.erase(key); }

    void clear() { this->delegate.clear(); }

    [[nodiscard]] size_t size() {
        this->purgeExpired();
        return this->delegate.size();
    }

    [[nodiscard]] std::vector<V> values() {
        this->purgeExpired();
        std::vector<V> result;
        result.reserve(this->delegate.size());
        for (const auto& [key, wrapper] : this->delegate) {
            result.push_back(wrapper.value);
        }
        return result;
    }

    [[nodiscard]] std::vector<K> keys() {
        this->purgeExpired();
        std::vector<K> result;
        result.reserve(this->delegate.size());
        for (const auto& [key, wrapper] : this->delegate) {
            result.push_back(key);
        }
        return result;
    }

    template <typename Fn>
    void forEach(Fn callback) {
        this->purgeExpired();
        for (auto& [key, wrapper] : this->delegate) {
            callback(wrapper.value, key);
        }
    }
};

} // namespace streamr::utils
