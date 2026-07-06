// Unit suite for the from-scratch network simulator
// (trackerless-network-completion-plan.md, phase 0.3). The simulator is
// test infrastructure everything later stands on, so its guarantees —
// delivery, per-association FIFO ordering, latency, close semantics —
// are pinned here against small stub connections, independent of the
// connection layer built on top of it.
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

import streamr.dht.Identifiers;
import streamr.dht.Simulator;
import streamr.dht.SimulatorInterfaces;
import streamr.dht.TestUtils;

using ::dht::PeerDescriptor;
using streamr::dht::connection::simulator::ISimulatorConnection;
using streamr::dht::connection::simulator::ISimulatorConnector;
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::testutils::createMockPeerDescriptor;

namespace {

// Test double recording everything the simulator delivers to it.
class StubConnection : public ISimulatorConnection {
public:
    PeerDescriptor local;
    PeerDescriptor remote;

    std::mutex mutex;
    std::condition_variable condition;
    std::vector<std::vector<std::byte>> receivedData;
    std::vector<std::chrono::steady_clock::time_point> receivedAt;
    bool disconnected = false;

    StubConnection(PeerDescriptor local, PeerDescriptor remote)
        : local(std::move(local)), remote(std::move(remote)) {}

    void handleIncomingData(const std::vector<std::byte>& data) override {
        std::scoped_lock lock(this->mutex);
        this->receivedData.push_back(data);
        this->receivedAt.push_back(std::chrono::steady_clock::now());
        this->condition.notify_all();
    }

    void handleIncomingDisconnection() override {
        std::scoped_lock lock(this->mutex);
        this->disconnected = true;
        this->condition.notify_all();
    }

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const override {
        return this->local;
    }

    [[nodiscard]] PeerDescriptor getRemotePeerDescriptor() const override {
        return this->remote;
    }

    bool waitForData(size_t count, std::chrono::milliseconds timeout) {
        std::unique_lock lock(this->mutex);
        return this->condition.wait_for(lock, timeout, [this, count]() {
            return this->receivedData.size() >= count;
        });
    }

    bool waitForDisconnect(std::chrono::milliseconds timeout) {
        std::unique_lock lock(this->mutex);
        return this->condition.wait_for(
            lock, timeout, [this]() { return this->disconnected; });
    }
};

// Test double that accepts every incoming connect by creating a
// counterpart stub connection (what SimulatorConnector does for real
// connections).
class StubConnector : public ISimulatorConnector {
public:
    PeerDescriptor local;
    Simulator& simulator;

    std::mutex mutex;
    std::condition_variable condition;
    std::vector<std::shared_ptr<StubConnection>> acceptedConnections;

    StubConnector(PeerDescriptor local, Simulator& simulator)
        : local(std::move(local)), simulator(simulator) {}

    [[nodiscard]] PeerDescriptor getPeerDescriptor() const override {
        return this->local;
    }

    void handleIncomingConnection(
        const std::shared_ptr<ISimulatorConnection>& sourceConnection)
        override {
        auto connection = std::make_shared<StubConnection>(
            this->local, sourceConnection->getLocalPeerDescriptor());
        {
            std::scoped_lock lock(this->mutex);
            this->acceptedConnections.push_back(connection);
            this->condition.notify_all();
        }
        this->simulator.accept(sourceConnection, connection);
    }

    bool waitForConnections(size_t count, std::chrono::milliseconds timeout) {
        std::unique_lock lock(this->mutex);
        return this->condition.wait_for(lock, timeout, [this, count]() {
            return this->acceptedConnections.size() >= count;
        });
    }
};

// Helper waiting for the connectedCallback of Simulator::connect.
struct ConnectResult {
    std::mutex mutex;
    std::condition_variable condition;
    bool called = false;
    std::optional<std::string> error;

    void set(const std::optional<std::string>& err) {
        std::scoped_lock lock(this->mutex);
        this->called = true;
        this->error = err;
        this->condition.notify_all();
    }

    bool wait(std::chrono::milliseconds timeout) {
        std::unique_lock lock(this->mutex);
        return this->condition.wait_for(
            lock, timeout, [this]() { return this->called; });
    }
};

std::vector<std::byte> makeData(uint8_t value) {
    return {std::byte{value}};
}

using namespace std::chrono_literals;
constexpr auto testTimeout = 5000ms;

// Establishes source->target connectivity through connector acceptance
// and returns once both directions are wired.
std::shared_ptr<StubConnection> establishConnection(
    Simulator& simulator,
    const std::shared_ptr<StubConnection>& sourceConnection,
    StubConnector& targetConnector) {
    ConnectResult result;
    simulator.connect(
        sourceConnection,
        targetConnector.getPeerDescriptor(),
        [&result](const std::optional<std::string>& error) {
            result.set(error);
        });
    if (!result.wait(testTimeout)) {
        return nullptr;
    }
    if (result.error.has_value()) {
        return nullptr;
    }
    return targetConnector.acceptedConnections.at(0);
}

} // namespace

TEST(SimulatorTest, ConnectDeliversIncomingConnectionAndConnectedCallback) {
    Simulator simulator;
    const auto descriptor1 = createMockPeerDescriptor();
    const auto descriptor2 = createMockPeerDescriptor();
    auto connector2 = std::make_shared<StubConnector>(descriptor2, simulator);
    simulator.addConnector(connector2);

    auto connection1 =
        std::make_shared<StubConnection>(descriptor1, descriptor2);
    auto accepted = establishConnection(simulator, connection1, *connector2);
    ASSERT_NE(accepted, nullptr);
    EXPECT_EQ(
        accepted->getRemotePeerDescriptor().nodeid(), descriptor1.nodeid());
}

TEST(SimulatorTest, ConnectToUnknownTargetReportsError) {
    Simulator simulator;
    const auto descriptor1 = createMockPeerDescriptor();
    const auto unknownTarget = createMockPeerDescriptor();

    auto connection1 =
        std::make_shared<StubConnection>(descriptor1, unknownTarget);
    ConnectResult result;
    simulator.connect(
        connection1,
        unknownTarget,
        [&result](const std::optional<std::string>& error) {
            result.set(error);
        });
    ASSERT_TRUE(result.wait(testTimeout));
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error.value(), "Target connector not found");
}

TEST(SimulatorTest, SendDeliversDataInBothDirections) {
    Simulator simulator;
    const auto descriptor1 = createMockPeerDescriptor();
    const auto descriptor2 = createMockPeerDescriptor();
    auto connector2 = std::make_shared<StubConnector>(descriptor2, simulator);
    simulator.addConnector(connector2);

    auto connection1 =
        std::make_shared<StubConnection>(descriptor1, descriptor2);
    auto connection2 = establishConnection(simulator, connection1, *connector2);
    ASSERT_NE(connection2, nullptr);

    simulator.send(*connection1, makeData(1));
    ASSERT_TRUE(connection2->waitForData(1, testTimeout));
    EXPECT_EQ(connection2->receivedData.at(0), makeData(1));

    simulator.send(*connection2, makeData(2));
    ASSERT_TRUE(connection1->waitForData(1, testTimeout));
    EXPECT_EQ(connection1->receivedData.at(0), makeData(2));
}

TEST(SimulatorTest, MessagesArriveInSendOrder) {
    // FIXED latency exercises the deadline queue; the per-association
    // FIFO clamp must still keep the send order.
    Simulator simulator(LatencyType::FIXED, 10);
    const auto descriptor1 = createMockPeerDescriptor();
    const auto descriptor2 = createMockPeerDescriptor();
    auto connector2 = std::make_shared<StubConnector>(descriptor2, simulator);
    simulator.addConnector(connector2);

    auto connection1 =
        std::make_shared<StubConnection>(descriptor1, descriptor2);
    auto connection2 = establishConnection(simulator, connection1, *connector2);
    ASSERT_NE(connection2, nullptr);

    constexpr size_t messageCount = 100;
    for (size_t i = 0; i < messageCount; i++) {
        simulator.send(*connection1, makeData(static_cast<uint8_t>(i)));
    }
    ASSERT_TRUE(connection2->waitForData(messageCount, testTimeout));
    for (size_t i = 0; i < messageCount; i++) {
        EXPECT_EQ(
            connection2->receivedData.at(i), makeData(static_cast<uint8_t>(i)));
    }
}

TEST(SimulatorTest, FixedLatencyDelaysDelivery) {
    constexpr auto latency = 100ms;
    Simulator simulator(LatencyType::FIXED, latency.count());
    const auto descriptor1 = createMockPeerDescriptor();
    const auto descriptor2 = createMockPeerDescriptor();
    auto connector2 = std::make_shared<StubConnector>(descriptor2, simulator);
    simulator.addConnector(connector2);

    auto connection1 =
        std::make_shared<StubConnection>(descriptor1, descriptor2);
    auto connection2 = establishConnection(simulator, connection1, *connector2);
    ASSERT_NE(connection2, nullptr);

    const auto sentAt = std::chrono::steady_clock::now();
    simulator.send(*connection1, makeData(1));
    ASSERT_TRUE(connection2->waitForData(1, testTimeout));
    EXPECT_GE(connection2->receivedAt.at(0) - sentAt, latency);
}

TEST(SimulatorTest, CloseDisconnectsTheCounterpart) {
    Simulator simulator;
    const auto descriptor1 = createMockPeerDescriptor();
    const auto descriptor2 = createMockPeerDescriptor();
    auto connector2 = std::make_shared<StubConnector>(descriptor2, simulator);
    simulator.addConnector(connector2);

    auto connection1 =
        std::make_shared<StubConnection>(descriptor1, descriptor2);
    auto connection2 = establishConnection(simulator, connection1, *connector2);
    ASSERT_NE(connection2, nullptr);

    simulator.close(*connection1);
    ASSERT_TRUE(connection2->waitForDisconnect(testTimeout));
}

TEST(SimulatorTest, SendAfterCloseIsDropped) {
    Simulator simulator;
    const auto descriptor1 = createMockPeerDescriptor();
    const auto descriptor2 = createMockPeerDescriptor();
    auto connector2 = std::make_shared<StubConnector>(descriptor2, simulator);
    simulator.addConnector(connector2);

    auto connection1 =
        std::make_shared<StubConnection>(descriptor1, descriptor2);
    auto connection2 = establishConnection(simulator, connection1, *connector2);
    ASSERT_NE(connection2, nullptr);

    simulator.close(*connection1);
    simulator.send(*connection1, makeData(1));
    ASSERT_TRUE(connection2->waitForDisconnect(testTimeout));
    EXPECT_EQ(connection2->receivedData.size(), 0);
}

TEST(SimulatorTest, FixedLatencyRequiresAValue) {
    EXPECT_THROW(Simulator{LatencyType::FIXED}, std::runtime_error);
}

TEST(SimulatorTest, RealLatencyRejectsInvalidRegions) {
    Simulator simulator(LatencyType::REAL);
    auto descriptor1 = createMockPeerDescriptor();
    descriptor1.set_region(99);
    const auto descriptor2 = createMockPeerDescriptor();

    auto connection1 =
        std::make_shared<StubConnection>(descriptor1, descriptor2);
    EXPECT_THROW(
        simulator.connect(
            connection1, descriptor2, [](const std::optional<std::string>&) {}),
        std::runtime_error);
}

TEST(SimulatorTest, StoppedSimulatorDropsOperations) {
    Simulator simulator;
    const auto descriptor1 = createMockPeerDescriptor();
    const auto descriptor2 = createMockPeerDescriptor();
    auto connector2 = std::make_shared<StubConnector>(descriptor2, simulator);
    simulator.addConnector(connector2);

    auto connection1 =
        std::make_shared<StubConnection>(descriptor1, descriptor2);
    auto connection2 = establishConnection(simulator, connection1, *connector2);
    ASSERT_NE(connection2, nullptr);

    simulator.stop();
    simulator.send(*connection1, makeData(1));
    // no delivery may happen after stop; give the (stopped) dispatcher a
    // moment to prove it
    EXPECT_FALSE(connection2->waitForData(1, 100ms));
}
