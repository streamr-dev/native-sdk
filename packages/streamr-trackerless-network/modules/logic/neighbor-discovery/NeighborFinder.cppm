// Module streamr.trackerlessnetwork.NeighborFinder
// Ported from packages/trackerless-network/src/content-delivery-layer/
// neighbor-discovery/NeighborFinder.ts (v103.8.0-rc.3): drives repeated
// doFindNeighbors rounds (250 ms apart, two concurrent chains) until the
// neighbor list reaches minCount or the contact views are exhausted.
//
// Adaptation: the TS recursion via setAbortableTimeout becomes timer
// callbacks that enqueue the next round on a GuardedAsyncScope (executor-
// attached per the scope's contract); each round is bounded by
// doFindNeighbors itself, and stop() aborts the timers before draining
// the scope.
module;

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <atomic>
#include <chrono>
#include <functional>
#include <set>
#include <vector>

export module streamr.trackerlessnetwork.NeighborFinder;

import streamr.utils.CoroutineHelper;
import streamr.utils.AbortController;
import streamr.utils.AbortableTimers;
import streamr.utils.GuardedAsyncScope;
import streamr.utils.SharedExecutors;
import streamr.trackerlessnetwork.NodeList;
import streamr.dht.Identifiers;
import streamr.logger.SLogger;

// Hoisted (file scope, NOT exported); fully qualified because relative
// namespace names resolve differently at file scope than inside the
// package namespace.
using streamr::dht::DhtAddress;
using streamr::logger::SLogger;
using streamr::utils::AbortableTimers;
using streamr::utils::AbortController;
using streamr::utils::GuardedAsyncScope;
using streamr::utils::SharedSerialExecutor;

export namespace streamr::trackerlessnetwork::neighbordiscovery {

constexpr std::chrono::milliseconds neighborFinderInitialWait{100};
constexpr std::chrono::milliseconds neighborFinderInterval{250};

// Small seam so NeighborUpdateRpcLocal / NeighborUpdateManager can be
// unit-tested with a counting double (the TS tests inject
// `{ start: jest.fn() }`).
class INeighborFinder {
public:
    virtual ~INeighborFinder() = default;
    virtual void start(std::vector<DhtAddress> excluded) = 0;
    void start() { this->start({}); }
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;
};

struct NeighborFinderOptions {
    NodeList& neighbors;
    NodeList& nearbyNodeView;
    NodeList& leftNodeView;
    NodeList& rightNodeView;
    NodeList& randomNodeView;
    std::function<folly::coro::Task<std::vector<DhtAddress>>(
        std::vector<DhtAddress>)>
        doFindNeighbors;
    size_t minCount;
};

class NeighborFinder : public INeighborFinder {
private:
    NeighborFinderOptions options;
    AbortController abortController;
    // Serial view so the rounds of the two chains never overlap; the
    // scope drain in stop() is bounded because doFindNeighbors is (its
    // RPCs carry timeouts).
    SharedSerialExecutor executor{streamr::utils::SharedExecutors::worker()};
    GuardedAsyncScope scope;
    std::atomic<bool> running = false;

public:
    explicit NeighborFinder(NeighborFinderOptions options)
        : options(std::move(options)) {}

    ~NeighborFinder() override { this->stop(); }

    using INeighborFinder::start;

    void start(std::vector<DhtAddress> excluded) override {
        if (this->running.exchange(true)) {
            return;
        }
        AbortableTimers::setAbortableTimeout(
            [this, excluded]() {
                // TS runs two concurrent find chains.
                this->scheduleFindNeighbors(excluded);
                this->scheduleFindNeighbors(excluded);
            },
            neighborFinderInitialWait,
            this->abortController.getSignal());
    }

    void stop() override {
        if (!this->running.exchange(false)) {
            return;
        }
        this->abortController.abort();
        this->scope.close();
    }

    [[nodiscard]] bool isRunning() const override {
        return this->running.load();
    }

private:
    void scheduleFindNeighbors(std::vector<DhtAddress> excluded) {
        this->scope.add(
            streamr::utils::co_withExecutor(
                &this->executor,
                folly::coro::co_invoke(
                    [this, excluded = std::move(excluded)]() mutable
                        -> folly::coro::Task<void> {
                        co_await this->findNeighbors(std::move(excluded));
                    })));
    }

    folly::coro::Task<void> findNeighbors(std::vector<DhtAddress> excluded) {
        if (!this->running.load()) {
            co_return;
        }
        auto newExcludes =
            co_await this->options.doFindNeighbors(std::move(excluded));
        std::set<DhtAddress> uniqueContacts;
        for (const auto* view :
             {&this->options.nearbyNodeView,
              &this->options.leftNodeView,
              &this->options.rightNodeView,
              &this->options.randomNodeView}) {
            for (const auto& id : view->getIds()) {
                uniqueContacts.insert(id);
            }
        }
        const auto uniqueContactCount = uniqueContacts.size();
        if (this->options.neighbors.size() < this->options.minCount &&
            newExcludes.size() < uniqueContactCount) {
            AbortableTimers::setAbortableTimeout(
                [this, newExcludes]() {
                    this->scheduleFindNeighbors(newExcludes);
                },
                neighborFinderInterval,
                this->abortController.getSignal());
        } else if (
            this->options.neighbors.size() == 0 && uniqueContactCount > 0) {
            SLogger::debug(
                "No neighbors found yet contacts are available, restarting "
                "handshaking process");
            AbortableTimers::setAbortableTimeout(
                [this]() { this->scheduleFindNeighbors({}); },
                neighborFinderInterval,
                this->abortController.getSignal());
        } else {
            this->running.store(false);
        }
    }
};

} // namespace streamr::trackerlessnetwork::neighbordiscovery
