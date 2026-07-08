// Module streamr.dht.Simulator
// In-memory network simulator for tests: many nodes' ConnectionManagers
// exchange messages in one process, with optional artificial latency.
//
// The API mirrors the TS Simulator
// (packages/dht/src/connection/simulator/Simulator.ts, v103.8.0-rc.3) so
// ported tests translate mechanically, but the implementation is written
// from scratch for a threaded runtime (trackerless-network-completion-
// plan.md, decision 3.1, option b):
//  - a single dispatcher thread owns ALL message delivery; operations
//    are ordered by (deadline, sequence number) in a priority queue, so
//    delivery order is deterministic for equal deadlines;
//  - per-association FIFO is enforced the same way as in TS: an
//    operation's deadline is clamped to be >= the association's previous
//    operation's deadline (lastOperationAt);
//  - the simulator mutex is NEVER held while calling into connections or
//    connectors: the downstream code (handshakers, ConnectionManager)
//    takes its own locks and may call back into the simulator, so
//    holding the mutex across call-outs would invite lock-order
//    deadlocks between the dispatcher and user threads.
//
// Known deviation from TS: a PeerDescriptor with no region set cannot be
// distinguished from region 0 (proto3 default); with LatencyType::REAL
// the TS version throws for undefined regions, the C++ version treats
// them as region 0. Regions > 15 throw in both.
module;

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

#include <string>

export module streamr.dht.Simulator;

import streamr.dht.protos;

import streamr.dht.Identifiers;
import streamr.dht.RegionPings;
import streamr.dht.SimulatorInterfaces;
import streamr.logger.SLogger;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;

export namespace streamr::dht::connection::simulator {

using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;

enum class LatencyType : std::uint8_t { NONE, RANDOM, REAL, FIXED };

class Simulator {
private:
    using Clock = std::chrono::steady_clock;

    // One-way 'pipe' of messages (same concept as the TS Association).
    struct Association {
        std::shared_ptr<ISimulatorConnection> sourceConnection;
        std::shared_ptr<ISimulatorConnection> destinationConnection; // null
                                                                     // until
                                                                     // accepted
        std::function<void(const std::optional<std::string>& error)>
            connectedCallback; // only on the connecting side
        Clock::time_point lastOperationAt{};
        bool closing = false;
    };

    enum class OperationType : std::uint8_t { CONNECT, SEND, CLOSE };

    struct Operation {
        Clock::time_point executionTime;
        uint64_t sequenceNumber;
        OperationType type;
        std::shared_ptr<Association> association;
        std::vector<std::byte> data; // SEND only
        PeerDescriptor targetDescriptor; // CONNECT only
    };

    struct OperationOrder {
        bool operator()(const Operation& a, const Operation& b) const {
            if (a.executionTime == b.executionTime) {
                return a.sequenceNumber > b.sequenceNumber;
            }
            return a.executionTime > b.executionTime;
        }
    };

    LatencyType latencyType;
    double fixedLatencyMs = 0;
    std::array<std::array<double, regionCount>, regionCount> latencyTable{};
    std::mt19937 randomGenerator{std::random_device{}()};

    // All fields below are protected by mMutex. The mutex is plain (not
    // recursive) on purpose: every call-out into connections/connectors
    // must first release it (see the module comment).
    std::mutex mMutex;
    std::condition_variable mCondition;
    bool stopped = false;
    uint64_t nextSequenceNumber = 0;
    std::map<DhtAddress, std::shared_ptr<ISimulatorConnector>> connectors;
    std::map<const ISimulatorConnection*, std::shared_ptr<Association>>
        associations;
    std::priority_queue<Operation, std::vector<Operation>, OperationOrder>
        operationQueue;

    std::thread dispatcherThread;

    [[nodiscard]] double getLatencyMs(
        uint32_t sourceRegion, uint32_t targetRegion) {
        switch (this->latencyType) {
            case LatencyType::NONE:
                return 0;
            case LatencyType::FIXED:
                return this->fixedLatencyMs;
            case LatencyType::RANDOM: {
                // NOLINTNEXTLINE(readability-magic-numbers)
                std::uniform_real_distribution<double> distribution(5, 250);
                return distribution(this->randomGenerator);
            }
            case LatencyType::REAL: {
                if (sourceRegion >= regionCount ||
                    targetRegion >= regionCount) {
                    SLogger::error("invalid region index given to Simulator");
                    throw std::runtime_error(
                        "invalid region index given to Simulator");
                }
                return this->latencyTable[sourceRegion][targetRegion];
            }
        }
        return 0;
    }

    // Deadline = now + latency, clamped so operations on one association
    // never overtake each other (per-association FIFO).
    [[nodiscard]] Clock::time_point generateExecutionTime(
        const std::shared_ptr<Association>& association,
        uint32_t sourceRegion,
        uint32_t targetRegion) {
        auto executionTime =
            Clock::now() +
            std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double, std::milli>(
                    this->getLatencyMs(sourceRegion, targetRegion)));
        if (association->lastOperationAt > executionTime) {
            executionTime = association->lastOperationAt;
        }
        return executionTime;
    }

    void scheduleOperation(Operation&& operation) {
        this->operationQueue.push(std::move(operation));
        this->mCondition.notify_all();
    }

    // Called with the lock held; unlocks around call-outs.
    void executeConnectOperation(
        const Operation& operation, std::unique_lock<std::mutex>& lock) {
        const auto targetNodeId = Identifiers::getNodeIdFromPeerDescriptor(
            operation.targetDescriptor);
        const auto connectorIterator = this->connectors.find(targetNodeId);
        if (connectorIterator == this->connectors.end()) {
            SLogger::error(
                "Target connector not found when executing connect operation");
            const auto callback = operation.association->connectedCallback;
            lock.unlock();
            if (callback) {
                callback("Target connector not found");
            }
            lock.lock();
            return;
        }
        const auto connector = connectorIterator->second;
        const auto sourceConnection = operation.association->sourceConnection;
        lock.unlock();
        connector->handleIncomingConnection(sourceConnection);
        lock.lock();
    }

    // Called with the lock held; unlocks around call-outs. Mirrors the
    // TS close/ack sequence: the first CloseOperation disconnects the
    // counterpart and schedules a CloseOperation back (the 'ack'), which
    // finally removes both associations.
    void executeCloseOperation(
        const Operation& operation, std::unique_lock<std::mutex>& lock) {
        const auto target = operation.association->destinationConnection;
        std::shared_ptr<Association> counterAssociation;
        if (target) {
            const auto counterIterator = this->associations.find(target.get());
            if (counterIterator != this->associations.end()) {
                counterAssociation = counterIterator->second;
            }
        }
        if (!target || !counterAssociation) {
            this->associations.erase(
                operation.association->sourceConnection.get());
        } else if (!counterAssociation->closing) {
            lock.unlock();
            target->handleIncomingDisconnection();
            this->close(*target);
            lock.lock();
        } else {
            // this is the 'ack' of the CloseOperation to the original
            // closer
            this->associations.erase(target.get());
            this->associations.erase(
                operation.association->sourceConnection.get());
        }
    }

    // Called with the lock held; unlocks around call-outs.
    void executeSendOperation(
        const Operation& operation, std::unique_lock<std::mutex>& lock) {
        const auto destination = operation.association->destinationConnection;
        if (!destination) {
            SLogger::error(
                "send operation executed on an association with no"
                " destination, dropping the message");
            return;
        }
        lock.unlock();
        destination->handleIncomingData(operation.data);
        lock.lock();
    }

    void dispatchLoop() {
        std::unique_lock<std::mutex> lock(this->mMutex);
        while (!this->stopped) {
            if (this->operationQueue.empty()) {
                this->mCondition.wait(lock, [this]() {
                    return this->stopped || !this->operationQueue.empty();
                });
                continue;
            }
            const auto nextDeadline = this->operationQueue.top().executionTime;
            if (Clock::now() < nextDeadline) {
                // Woken early by new operations (possibly with earlier
                // deadlines) or stop(); loop re-evaluates either way.
                this->mCondition.wait_until(lock, nextDeadline);
                continue;
            }
            const Operation operation = this->operationQueue.top();
            this->operationQueue.pop();
            switch (operation.type) {
                case OperationType::CONNECT:
                    this->executeConnectOperation(operation, lock);
                    break;
                case OperationType::CLOSE:
                    this->executeCloseOperation(operation, lock);
                    break;
                case OperationType::SEND:
                    this->executeSendOperation(operation, lock);
                    break;
            }
        }
    }

public:
    explicit Simulator(
        LatencyType latencyType = LatencyType::NONE,
        std::optional<double> fixedLatencyMs = std::nullopt)
        : latencyType(latencyType) {
        if (latencyType == LatencyType::REAL) {
            this->latencyTable = getRegionDelayMatrix();
        }
        if (latencyType == LatencyType::FIXED) {
            if (!fixedLatencyMs.has_value()) {
                throw std::runtime_error(
                    "LatencyType::FIXED requires the desired latency to be"
                    " given as second parameter");
            }
            this->fixedLatencyMs = fixedLatencyMs.value();
        }
        this->dispatcherThread =
            std::thread([this]() { this->dispatchLoop(); });
    }

    ~Simulator() { this->stop(); }

    Simulator(const Simulator&) = delete;
    Simulator& operator=(const Simulator&) = delete;
    Simulator(Simulator&&) = delete;
    Simulator& operator=(Simulator&&) = delete;

    void addConnector(const std::shared_ptr<ISimulatorConnector>& connector) {
        std::scoped_lock lock(this->mMutex);
        this->connectors.emplace(
            Identifiers::getNodeIdFromPeerDescriptor(
                connector->getPeerDescriptor()),
            connector);
    }

    // Called by the target-side connector (from the dispatcher thread,
    // via handleIncomingConnection) once it has created the server-side
    // connection for an incoming connect.
    void accept(
        const std::shared_ptr<ISimulatorConnection>& sourceConnection,
        const std::shared_ptr<ISimulatorConnection>& targetConnection) {
        std::function<void(const std::optional<std::string>&)> callback;
        {
            std::scoped_lock lock(this->mMutex);
            const auto sourceIterator =
                this->associations.find(sourceConnection.get());
            if (sourceIterator == this->associations.end()) {
                SLogger::error("source association not found in accept()");
                return;
            }
            sourceIterator->second->destinationConnection = targetConnection;
            auto targetAssociation = std::make_shared<Association>();
            targetAssociation->sourceConnection = targetConnection;
            targetAssociation->destinationConnection = sourceConnection;
            this->associations.emplace(
                targetConnection.get(), std::move(targetAssociation));
            callback = sourceIterator->second->connectedCallback;
        }
        if (callback) {
            callback(std::nullopt);
        }
    }

    void connect(
        const std::shared_ptr<ISimulatorConnection>& sourceConnection,
        const PeerDescriptor& targetDescriptor,
        std::function<void(const std::optional<std::string>& error)>
            connectedCallback) {
        std::scoped_lock lock(this->mMutex);
        if (this->stopped) {
            SLogger::error("connect() called on a stopped simulator");
            return;
        }
        auto association = std::make_shared<Association>();
        association->sourceConnection = sourceConnection;
        association->connectedCallback = std::move(connectedCallback);
        this->associations[sourceConnection.get()] = association;

        const auto executionTime = this->generateExecutionTime(
            association,
            sourceConnection->getLocalPeerDescriptor().region(),
            targetDescriptor.region());
        association->lastOperationAt = executionTime;
        this->scheduleOperation(
            Operation{
                .executionTime = executionTime,
                .sequenceNumber = this->nextSequenceNumber++,
                .type = OperationType::CONNECT,
                .association = std::move(association),
                .targetDescriptor = targetDescriptor});
    }

    void send(
        const ISimulatorConnection& sourceConnection,
        const std::vector<std::byte>& data) {
        std::scoped_lock lock(this->mMutex);
        if (this->stopped) {
            return;
        }
        const auto associationIterator =
            this->associations.find(&sourceConnection);
        if (associationIterator == this->associations.end()) {
            return;
        }
        const auto& association = associationIterator->second;
        if (association->closing) {
            SLogger::trace("Tried to call send() on a closing association");
            return;
        }
        const auto targetRegion = association->destinationConnection
            ? association->destinationConnection->getLocalPeerDescriptor()
                  .region()
            : sourceConnection.getRemotePeerDescriptor().region();
        const auto executionTime = this->generateExecutionTime(
            association,
            sourceConnection.getLocalPeerDescriptor().region(),
            targetRegion);
        association->lastOperationAt = executionTime;
        this->scheduleOperation(
            Operation{
                .executionTime = executionTime,
                .sequenceNumber = this->nextSequenceNumber++,
                .type = OperationType::SEND,
                .association = association,
                .data = data});
    }

    void close(const ISimulatorConnection& sourceConnection) {
        std::scoped_lock lock(this->mMutex);
        if (this->stopped) {
            return;
        }
        const auto associationIterator =
            this->associations.find(&sourceConnection);
        if (associationIterator == this->associations.end()) {
            return;
        }
        const auto& association = associationIterator->second;
        association->closing = true;
        const auto executionTime = this->generateExecutionTime(
            association,
            sourceConnection.getLocalPeerDescriptor().region(),
            sourceConnection.getRemotePeerDescriptor().region());
        association->lastOperationAt = executionTime;
        this->scheduleOperation(
            Operation{
                .executionTime = executionTime,
                .sequenceNumber = this->nextSequenceNumber++,
                .type = OperationType::CLOSE,
                .association = association});
    }

    void stop() {
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped) {
                return;
            }
            this->stopped = true;
            SLogger::info(
                std::to_string(this->associations.size()) +
                " associations in the beginning of stop()");
        }
        this->mCondition.notify_all();
        if (this->dispatcherThread.joinable() &&
            std::this_thread::get_id() != this->dispatcherThread.get_id()) {
            this->dispatcherThread.join();
        }
    }
};

} // namespace streamr::dht::connection::simulator
