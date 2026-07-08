// Module streamr.trackerlessnetwork.RandomAccessQueue
// CONSOLIDATED from the former header logic/propagation/RandomAccessQueue.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;
#include <new>


export module streamr.trackerlessnetwork.RandomAccessQueue;

import std;
export namespace streamr::trackerlessnetwork::propagation {

class QueueToken {
private:
    std::size_t id;
    explicit QueueToken(std::size_t id) : id(id) {}

public:
    QueueToken() : id(0) {} // create a non-existent token by default
    QueueToken(const QueueToken&) = default;
    QueueToken& operator=(const QueueToken&) = default;
    QueueToken(QueueToken&&) = default;
    QueueToken& operator=(QueueToken&&) = default;

    static QueueToken create() {
        // Use a "magic static"
        // https://blog.mbedded.ninja/programming/languages/c-plus-plus/magic-statics/
        static std::size_t counter = 1;
        static std::mutex queueTokenReferenceCounterMutex;
        std::lock_guard<std::mutex> lock{queueTokenReferenceCounterMutex};
        return QueueToken(counter++);
    }
    [[nodiscard]] std::size_t getId() const { return this->id; }
};

template <typename ValueType>
class RandomAccessQueue {
private:
    std::map<std::size_t, ValueType> queue;
    std::recursive_mutex queueMutex;

public:
    QueueToken push(const ValueType& value) {
        std::scoped_lock<std::recursive_mutex> lock{this->queueMutex};
        auto token = QueueToken::create();
        this->queue[token.getId()] = value;
        return token;
    }

    std::optional<ValueType> pop() {
        std::scoped_lock<std::recursive_mutex> lock{this->queueMutex};

        if (this->queue.empty()) {
            return std::nullopt;
        }

        auto handlerIterator = this->queue.begin();
        auto handler = handlerIterator->second;

        this->queue.erase(handlerIterator);

        return handler;
    }

    std::optional<ValueType> get(const QueueToken& token) {
        std::scoped_lock<std::recursive_mutex> lock{this->queueMutex};

        auto it = this->queue.find(token.getId());
        if (it == this->queue.end()) {
            return std::nullopt;
        }

        return it->second;
    }

    void remove(const QueueToken& token) {
        std::scoped_lock<std::recursive_mutex> lock{this->queueMutex};

        this->queue.erase(token.getId());
    }
};

} // namespace streamr::trackerlessnetwork::propagation
