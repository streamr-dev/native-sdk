// Ported from packages/dht/test/integration/ConnectionManager.test.ts
// (v103.8.0-rc.3), 'Connects and disconnects over simulated connections'
// case. Adaptation: the TS test calls the private closeConnection()
// through a @ts-expect-error escape hatch; here the disconnect is
// triggered through the public API by stopping one ConnectionManager,
// which gracefully closes the connection so both sides observe the
// Disconnected event.
//
// The TS file's other cases exercise the websocket connectors
// (connectivity checking, server start, nodeId validation) and are
// ported in milestone B (trackerless-network-completion-plan.md).
#include <atomic>
#include <functional>
#include <memory>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.Identifiers;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.TestUtils;
import streamr.dht.Transport;
import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;

using ::dht::Message;
using ::dht::PeerDescriptor;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::connection::simulator::SimulatorTransport;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::transport::SendOptions;
using streamr::utils::waitForCondition;

namespace transportevents = streamr::dht::transport::transportevents;

namespace {

constexpr auto serviceId = "demo";

void expectCondition(const char* label, std::function<bool()>&& condition) {
    SCOPED_TRACE(label);
    auto task = waitForCondition(std::move(condition));
    EXPECT_NO_THROW(streamr::utils::blockingWait(std::move(task)));
}

} // namespace

TEST(
    ConnectionManagerIntegrationTest,
    ConnectsAndDisconnectsOverSimulatedConnections) {
    const auto peerDescriptor3 = createMockPeerDescriptor();
    const auto peerDescriptor4 = createMockPeerDescriptor();

    Simulator simulator2;
    auto connectionManager3 =
        std::make_shared<SimulatorTransport>(peerDescriptor3, simulator2);
    connectionManager3->start();
    auto connectionManager4 =
        std::make_shared<SimulatorTransport>(peerDescriptor4, simulator2);
    connectionManager4->start();

    Message msg;
    msg.set_serviceid(serviceId);
    msg.set_messageid("1");
    msg.mutable_rpcmessage(); // empty RpcMessage body

    std::atomic<bool> dataReceived = false;
    std::atomic<bool> connected3 = false;
    std::atomic<bool> connected4 = false;
    std::atomic<bool> disconnected3 = false;
    std::atomic<bool> disconnected4 = false;

    connectionManager4->on<transportevents::Message>(
        [&dataReceived](const Message& message) {
            EXPECT_TRUE(message.has_rpcmessage());
            dataReceived = true;
        });
    connectionManager4->on<transportevents::Connected>(
        [&connected4](const PeerDescriptor& /*peerDescriptor*/) {
            connected4 = true;
        });
    connectionManager3->on<transportevents::Connected>(
        [&connected3](const PeerDescriptor& /*peerDescriptor*/) {
            connected3 = true;
        });
    connectionManager4->on<transportevents::Disconnected>(
        [&disconnected4](
            const PeerDescriptor& /*peerDescriptor*/, bool /*gracefulLeave*/) {
            disconnected4 = true;
        });
    connectionManager3->on<transportevents::Disconnected>(
        [&disconnected3](
            const PeerDescriptor& /*peerDescriptor*/, bool /*gracefulLeave*/) {
            disconnected3 = true;
        });

    msg.mutable_targetdescriptor()->CopyFrom(peerDescriptor4);
    connectionManager3->send(msg, SendOptions{});

    expectCondition(
        "dataReceived", [&dataReceived]() { return dataReceived.load(); });
    expectCondition(
        "connected3", [&connected3]() { return connected3.load(); });
    expectCondition(
        "connected4", [&connected4]() { return connected4.load(); });

    // disconnect through the public API (see the header comment)
    connectionManager3->stop();

    expectCondition(
        "disconnected3", [&disconnected3]() { return disconnected3.load(); });
    expectCondition(
        "disconnected4", [&disconnected4]() { return disconnected4.load(); });

    connectionManager4->stop();
    simulator2.stop();
}
