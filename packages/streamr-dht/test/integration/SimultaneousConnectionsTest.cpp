// Ported from packages/dht/test/integration/SimultaneousConnections.test.ts
// (v103.8.0-rc.3), 'simultanous simulated connection' case: two nodes
// connect to each other at the same time over the simulator, which
// forces the connection-tiebreak path in the endpoint state machine.
//
// The TS file's remaining cases (websocket 2 servers / websocket
// ConnectionRequests / WebRTC) exercise the real connectors and are
// ported in milestone B together with the other connector-level tests
// (trackerless-network-completion-plan.md).
#include <atomic>
#include <functional>
#include <memory>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.Identifiers;
import streamr.dht.RegionPings;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.TestUtils;
import streamr.dht.Transport;
import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;

using ::dht::Message;
using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::connection::simulator::getRandomRegion;
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::connection::simulator::SimulatorTransport;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::transport::SendOptions;
using streamr::utils::waitForCondition;

namespace transportevents = streamr::dht::transport::transportevents;

namespace {

Message createBaseMessage(const PeerDescriptor& targetDescriptor) {
    Message message;
    message.set_serviceid("serviceId");
    message.set_messageid("1");
    message.mutable_rpcmessage(); // empty RpcMessage body
    message.mutable_targetdescriptor()->CopyFrom(targetDescriptor);
    return message;
}

PeerDescriptor createPeerDescriptorWithRandomRegion() {
    auto descriptor = createMockPeerDescriptor();
    descriptor.set_region(static_cast<uint32_t>(getRandomRegion()));
    return descriptor;
}

void expectCondition(std::function<bool()>&& condition) {
    auto task = waitForCondition(std::move(condition));
    EXPECT_NO_THROW(streamr::utils::blockingWait(std::move(task)));
}

} // namespace

TEST(SimultaneousConnectionsTest, SimultaneousSimulatedConnection) {
    const auto peerDescriptor1 = createPeerDescriptorWithRandomRegion();
    const auto peerDescriptor2 = createPeerDescriptorWithRandomRegion();

    Simulator simulator(LatencyType::REAL);
    auto transport1 =
        std::make_shared<SimulatorTransport>(peerDescriptor1, simulator);
    transport1->start();
    auto transport2 =
        std::make_shared<SimulatorTransport>(peerDescriptor2, simulator);
    transport2->start();

    std::atomic<bool> received1 = false;
    std::atomic<bool> received2 = false;
    transport1->on<transportevents::Message>(
        [&received1](const Message& message) {
            EXPECT_TRUE(message.has_rpcmessage());
            received1 = true;
        });
    transport2->on<transportevents::Message>(
        [&received2](const Message& message) {
            EXPECT_TRUE(message.has_rpcmessage());
            received2 = true;
        });

    // both sides connect-and-send at the same time
    transport1->send(createBaseMessage(peerDescriptor2), SendOptions{});
    transport2->send(createBaseMessage(peerDescriptor1), SendOptions{});

    expectCondition([&received1]() { return received1.load(); });
    expectCondition([&received2]() { return received2.load(); });
    expectCondition([&transport2, &peerDescriptor1]() {
        return transport2->hasConnection(
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor1));
    });
    expectCondition([&transport1, &peerDescriptor2]() {
        return transport1->hasConnection(
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor2));
    });

    transport1->stop();
    transport2->stop();
    simulator.stop();
}
