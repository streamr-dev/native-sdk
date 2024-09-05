#ifndef STREAMR_DHT_DUPLICATE_DETECTOR_HPP
#define STREAMR_DHT_DUPLICATE_DETECTOR_HPP

#include <string>
#include <unordered_set>
#include <vector>

namespace streamr::dht::routing {

class DuplicateDetector {
private:
    std::unordered_set<std::string> values;
    std::vector<std::string> queue;
    size_t maxItemCount;

public:
    explicit DuplicateDetector(size_t maxItemCount)
        : maxItemCount(maxItemCount) {}
    virtual ~DuplicateDetector() = default;

    void add(const std::string& value) {
        this->values.insert(value);
        this->queue.push_back(value);
        if (this->queue.size() > this->maxItemCount) {
            const auto removed = this->queue.front();
            this->values.erase(removed);
            this->queue.erase(this->queue.begin());
        }
    }

    [[nodiscard]] bool isMostLikelyDuplicate(const std::string& value) const {
        return this->values.find(value) != this->values.end();
    }

    [[nodiscard]] size_t size() const { return this->values.size(); }

    void clear() { this->values.clear(); this->queue.clear(); }
};

} // namespace streamr::dht::routing

#endif
