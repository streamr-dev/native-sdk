// Module streamr.utils.SharedExecutors
//
// Process-wide worker pools replacing the former one-pool-per-instance
// executor members (PeerManager pings, DhtNode/PeerDiscovery recovery,
// Router routing, StoreManager replication, RpcCommunicator client/server
// dispatch). With per-instance pools the process thread count scaled
// linearly with the number of DHT nodes (~9 threads per node), which
// aborted the 200-node integration tests on the 3-core macOS CI runners
// with pthread_create EAGAIN ("thread constructor failed: Resource
// temporarily unavailable") — and would do the same to a real application
// running many layer-1 nodes (trackerless-network creates one per stream
// partition). TS runs all of this on one event loop; fixed-size shared
// pools are the C++ analogue.
//
// The two guarantees the per-instance pools used to give are preserved by
// the callers, not here:
//  - per-instance teardown ("no task outlives its owner", the former
//    executor-destructor/join()): each owner keeps its detached tasks in a
//    folly::coro::CancellableAsyncScope and drains it in stop()/its
//    destructor (the RpcCommunicatorServerApi pattern);
//  - single-threaded ordering (the former {1}-sized pools): owners that
//    relied on it wrap the shared pool in a per-instance
//    folly::SerialExecutor.
module;

#include <algorithm>
#include <memory>
#include <thread>

#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/SerialExecutor.h>
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <folly/executors/thread_factory/PriorityThreadFactory.h>

export module streamr.utils.SharedExecutors;

export namespace streamr::utils {

class SharedExecutors {
private:
    static constexpr unsigned int minWorkerThreads = 4;
    static constexpr unsigned int minBackgroundThreads = 2;
    // Matches the former per-Router pool's routingWorkerThreadNice.
    static constexpr int backgroundNice = 10;

public:
    // General detached work: pings, recovery rejoins, replication, RPC
    // dispatch. The floor keeps a few threads available even on small
    // machines so independent owners' work cannot serialize behind one
    // long-running task. Coroutines suspend rather than block while
    // awaiting I/O, so the pool size bounds CPU concurrency, not the
    // number of in-flight operations.
    static folly::CPUThreadPoolExecutor& worker() {
        // magic static
        static folly::CPUThreadPoolExecutor executor{
            std::max(minWorkerThreads, std::thread::hardware_concurrency()),
            std::make_shared<folly::NamedThreadFactory>("StreamrWorker")};
        return executor;
    }

    // Heavy second-class computations (the DHT routing-table work): low OS
    // priority so they never crowd out RPC dispatch — the phase-A4 design
    // point that routing offload must not block RPC/delivery threads.
    static folly::CPUThreadPoolExecutor& background() {
        // magic static
        static folly::CPUThreadPoolExecutor executor{
            std::max(
                minBackgroundThreads, std::thread::hardware_concurrency() / 2),
            std::make_shared<folly::PriorityThreadFactory>(
                std::make_shared<folly::NamedThreadFactory>(
                    "StreamrBackground"),
                backgroundNice)};
        return executor;
    }
};

// A per-owner SERIAL view of a shared pool: tasks posted through one
// instance run one at a time in FIFO order, on the pool's threads — the
// replacement for the former single-thread per-instance executors whose
// {1} size WAS their ordering guarantee. Implemented as a non-template
// folly::Executor wrapper so all of folly::SerialExecutor's template
// machinery (DistributedMutex innards) instantiates in THIS translation
// unit, where the headers are textually included — instantiating it from
// an importing module TU fails name lookup (the known BMI pitfall).
class SharedSerialExecutor final : public folly::Executor {
private:
    folly::Executor::KeepAlive<folly::SerialExecutor> impl;

public:
    explicit SharedSerialExecutor(folly::CPUThreadPoolExecutor& pool)
        : impl(folly::SerialExecutor::create(folly::getKeepAliveToken(pool))) {}

    void add(folly::Func func) override { this->impl->add(std::move(func)); }
};

} // namespace streamr::utils
