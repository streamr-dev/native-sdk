// Ported from packages/dht/test/integration/WebrtcConnectionManagement.test.ts
// (v103.8.0-rc.3): two ConnectionManagers whose peers have NO websocket
// server signal over a Simulator (FIXED 20 ms latency) and open real WebRTC
// data channels to each other. Adaptations: jest done-callbacks and event
// promises become waitForCondition on captured flags; the
// connects-and-disconnects case triggers the disconnect by stopping one
// manager through the public API instead of the TS @ts-expect-error call to
// the private closeConnection() (the ConnectionManagerIntegrationTest
// adaptation); the failed-connection case waits for the target's cleanup
// via hasConnection() like its websocket sibling.
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.ConnectionManager;
import streamr.dht.ConnectorFacade;
import streamr.dht.Errors;
import streamr.dht.Identifiers;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.TestUtils;
import streamr.dht.Transport;
import streamr.dht.protos;
import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;

using ::dht::ConnectivityResponse;
using ::dht::Message;
using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::connection::simulator::SimulatorTransport;
using streamr::dht::helpers::CannotConnectToSelf;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::utils::waitForCondition;

namespace transportevents = streamr::dht::transport::transportevents;

namespace {

constexpr auto serviceId = "dummy";
constexpr double fixedLatencyMs = 20;
// The TS cases run under 15-60 s jest timeouts; a real ICE + DTLS
// handshake over the loopback interface completes well within this.
constexpr auto webrtcConnectTimeout = std::chrono::seconds(30);
constexpr auto pollInterval = std::chrono::milliseconds(200);

Message createDummyMessage(
    const std::string& messageServiceId,
    const PeerDescriptor& targetDescriptor) {
    Message message;
    message.set_serviceid(messageServiceId);
    message.mutable_rpcmessage(); // body.oneofKind = rpcMessage
    message.set_messageid("mockerer");
    message.mutable_targetdescriptor()->CopyFrom(targetDescriptor);
    return message;
}

std::shared_ptr<ConnectionManager> createConnectionManager(
    const PeerDescriptor& localPeerDescriptor,
    streamr::dht::transport::Transport& transport) {
    return std::make_shared<ConnectionManager>(ConnectionManagerOptions{
        .createConnectorFacade = [localPeerDescriptor, &transport]()
            -> std::shared_ptr<DefaultConnectorFacade> {
            return std::make_shared<DefaultConnectorFacade>(
                DefaultConnectorFacadeOptions{
                    .transport = transport,
                    .createLocalPeerDescriptor =
                        [localPeerDescriptor](
                            const ConnectivityResponse& /*response*/)
                        -> PeerDescriptor { return localPeerDescriptor; }});
        }});
}

void expectCondition(
    const char* label,
    std::function<bool()>&& condition,
    std::chrono::milliseconds timeout = webrtcConnectTimeout) {
    SCOPED_TRACE(label);
    auto task = waitForCondition(std::move(condition), timeout, pollInterval);
    EXPECT_NO_THROW(streamr::utils::blockingWait(std::move(task)));
}

} // namespace

class WebrtcConnectionManagementTest : public ::testing::Test {
protected:
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    Simulator simulator{LatencyType::FIXED, fixedLatencyMs};
    PeerDescriptor peerDescriptor1;
    PeerDescriptor peerDescriptor2;
    std::shared_ptr<SimulatorTransport> connectorTransport1;
    std::shared_ptr<SimulatorTransport> connectorTransport2;
    std::shared_ptr<ConnectionManager> manager1;
    std::shared_ptr<ConnectionManager> manager2;
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    void SetUp() override {
        this->peerDescriptor1 = createMockPeerDescriptor();
        this->peerDescriptor2 = createMockPeerDescriptor();
        this->connectorTransport1 = std::make_shared<SimulatorTransport>(
            this->peerDescriptor1, this->simulator);
        this->connectorTransport1->start();
        this->manager1 = createConnectionManager(
            this->peerDescriptor1, *this->connectorTransport1);
        this->connectorTransport2 = std::make_shared<SimulatorTransport>(
            this->peerDescriptor2, this->simulator);
        this->connectorTransport2->start();
        this->manager2 = createConnectionManager(
            this->peerDescriptor2, *this->connectorTransport2);
        this->manager1->start();
        this->manager2->start();
    }

    void TearDown() override {
        this->manager1->stop();
        this->manager2->stop();
        this->connectorTransport1->stop();
        this->connectorTransport2->stop();
        this->simulator.stop();
    }
};

TEST_F(WebrtcConnectionManagementTest, Peer1CanOpenWebrtcDatachannels) {
    std::atomic<bool> received = false;
    this->manager2->on<transportevents::Message>(
        [&received](const Message& message) {
            EXPECT_EQ(message.messageid(), "mockerer");
            received = true;
        });
    this->manager1->send(
        createDummyMessage("unknown", this->peerDescriptor2), {});
    expectCondition("message received over webrtc", [&received]() {
        return received.load();
    });
}

TEST_F(WebrtcConnectionManagementTest, Peer2CanOpenWebrtcDatachannel) {
    std::atomic<bool> received = false;
    this->manager1->on<transportevents::Message>(
        [&received](const Message& message) {
            EXPECT_EQ(message.messageid(), "mockerer");
            received = true;
        });
    this->manager2->send(
        createDummyMessage(serviceId, this->peerDescriptor1), {});
    expectCondition("message received over webrtc", [&received]() {
        return received.load();
    });
}

TEST_F(WebrtcConnectionManagementTest, ConnectingToSelfThrows) {
    const auto selfMessage =
        createDummyMessage(serviceId, this->peerDescriptor1);
    EXPECT_THROW(this->manager1->send(selfMessage, {}), CannotConnectToSelf);
}

TEST_F(
    WebrtcConnectionManagementTest, ConnectsAndDisconnectsWebrtcConnections) {
    std::atomic<bool> dataReceived = false;
    std::atomic<bool> connected1 = false;
    std::atomic<bool> connected2 = false;
    std::atomic<bool> disconnected1 = false;
    std::atomic<bool> disconnected2 = false;

    this->manager2->on<transportevents::Message>(
        [&dataReceived](const Message& message) {
            EXPECT_TRUE(message.has_rpcmessage());
            dataReceived = true;
        });
    this->manager1->on<transportevents::Connected>(
        [&connected1](const PeerDescriptor& /*peer*/) { connected1 = true; });
    this->manager2->on<transportevents::Connected>(
        [&connected2](const PeerDescriptor& /*peer*/) { connected2 = true; });
    this->manager1->on<transportevents::Disconnected>(
        [&disconnected1](const PeerDescriptor& /*peer*/, bool /*graceful*/) {
            disconnected1 = true;
        });
    this->manager2->on<transportevents::Disconnected>(
        [&disconnected2](const PeerDescriptor& /*peer*/, bool /*graceful*/) {
            disconnected2 = true;
        });

    this->manager1->send(
        createDummyMessage(serviceId, this->peerDescriptor2), {});
    expectCondition("both connected and data flowed", [&]() {
        return dataReceived.load() && connected1.load() && connected2.load();
    });

    // TS closes the connection through the private closeConnection();
    // stopping one manager closes it through the public API.
    this->manager2->stop();
    expectCondition("both saw the disconnect", [&]() {
        return disconnected1.load() && disconnected2.load();
    });
}

TEST_F(WebrtcConnectionManagementTest, FailedConnectionsAreCleanedUp) {
    const auto targetDescriptor = createMockPeerDescriptor();
    const auto nodeId =
        Identifiers::getNodeIdFromPeerDescriptor(targetDescriptor);
    this->manager1->send(createDummyMessage(serviceId, targetDescriptor), {});
    auto* manager = this->manager1.get();
    expectCondition("failed connection cleaned up", [manager, nodeId]() {
        return !manager->hasConnection(nodeId);
    });
}
