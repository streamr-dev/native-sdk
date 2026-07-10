// Ported from packages/dht/test/integration/
// WebsocketConnectionManagement.test.ts (v103.8.0-rc.3): three
// ConnectionManagers signal over a shared Simulator; one runs a real
// websocket server, the other two are serverless. Opening a connection
// from the server peer to a serverless peer exercises the
// WebsocketServerConnector requestConnection path (the serverless peer is
// asked over the simulator to open a websocket back to the server);
// the reverse direction exercises the WebsocketClientConnector.
// Adaptations: jest done-callbacks and waitForEvent become
// waitForCondition on captured state; the failed-request cleanup case
// polls hasConnection() to false instead of racing a 'disconnected'
// waitForEvent against the send (the TS Promise.allSettled tolerates
// either finishing first, so the final assertion is the same).
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
import streamr.dht.PortRange;
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
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::connection::simulator::SimulatorTransport;
using streamr::dht::helpers::CannotConnectToSelf;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::types::PortRange;
using streamr::utils::waitForCondition;

namespace transportevents = streamr::dht::transport::transportevents;

namespace {

constexpr auto SERVICE_ID = "test";
constexpr uint16_t wsServerPort = 12223;
// The failed-request case waits out the 15 s PendingConnection connect
// watchdog (the TS test uses a 15 s waitForEvent inside a 20 s jest
// timeout).
constexpr auto failedRequestCleanupTimeout = std::chrono::seconds(20);
constexpr auto conditionPollInterval = std::chrono::milliseconds(200);

Message createDummyMessage(const PeerDescriptor& targetDescriptor) {
    Message message;
    message.set_serviceid(SERVICE_ID);
    message.mutable_rpcmessage(); // body.oneofKind = rpcMessage
    message.set_messageid("mockerer");
    message.mutable_targetdescriptor()->CopyFrom(targetDescriptor);
    return message;
}

std::shared_ptr<ConnectionManager> createConnectionManager(
    DefaultConnectorFacadeOptions options) {
    return std::make_shared<ConnectionManager>(ConnectionManagerOptions{
        .createConnectorFacade = [opts = std::move(options)]()
            -> std::shared_ptr<DefaultConnectorFacade> {
            return std::make_shared<DefaultConnectorFacade>(opts);
        }});
}

void expectCondition(
    const char* label,
    std::function<bool()>&& condition,
    std::chrono::milliseconds timeout = std::chrono::seconds(15)) {
    SCOPED_TRACE(label);
    auto task =
        waitForCondition(std::move(condition), timeout, conditionPollInterval);
    EXPECT_NO_THROW(streamr::utils::blockingWait(std::move(task)));
}

} // namespace

class WebsocketConnectionManagementTest : public ::testing::Test {
protected:
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    Simulator simulator;
    PeerDescriptor wsServerConnectorPeerDescriptor;
    PeerDescriptor noWsServerConnectorPeerDescriptor;
    PeerDescriptor biggerNoWsServerConnectorPeerDescriptor;
    std::shared_ptr<SimulatorTransport> connectorTransport1;
    std::shared_ptr<SimulatorTransport> connectorTransport2;
    std::shared_ptr<SimulatorTransport> connectorTransport3;
    std::shared_ptr<ConnectionManager> wsServerManager;
    std::shared_ptr<ConnectionManager> noWsServerManager;
    std::shared_ptr<ConnectionManager> biggerNoWsServerManager;
    // NOLINTEND(misc-non-private-member-variables-in-classes)

    void SetUp() override {
        this->wsServerConnectorPeerDescriptor = createMockPeerDescriptor();
        this->wsServerConnectorPeerDescriptor.mutable_websocket()->set_host(
            "127.0.0.1");
        this->wsServerConnectorPeerDescriptor.mutable_websocket()->set_port(
            wsServerPort);
        this->wsServerConnectorPeerDescriptor.mutable_websocket()->set_tls(
            false);
        this->noWsServerConnectorPeerDescriptor = createMockPeerDescriptor();
        this->biggerNoWsServerConnectorPeerDescriptor =
            createMockPeerDescriptor();

        this->connectorTransport1 = std::make_shared<SimulatorTransport>(
            this->wsServerConnectorPeerDescriptor, this->simulator);
        this->connectorTransport1->start();
        this->connectorTransport2 = std::make_shared<SimulatorTransport>(
            this->noWsServerConnectorPeerDescriptor, this->simulator);
        this->connectorTransport2->start();
        this->connectorTransport3 = std::make_shared<SimulatorTransport>(
            this->biggerNoWsServerConnectorPeerDescriptor, this->simulator);
        this->connectorTransport3->start();

        const auto wsServerDescriptor = this->wsServerConnectorPeerDescriptor;
        this->wsServerManager = createConnectionManager(
            DefaultConnectorFacadeOptions{
                .transport = *this->connectorTransport1,
                .websocketHost = "127.0.0.1",
                .websocketPortRange =
                    PortRange{.min = wsServerPort, .max = wsServerPort},
                .createLocalPeerDescriptor =
                    [wsServerDescriptor](
                        const ConnectivityResponse& /* response */)
                    -> PeerDescriptor { return wsServerDescriptor; }});

        const auto noWsServerDescriptor =
            this->noWsServerConnectorPeerDescriptor;
        this->noWsServerManager = createConnectionManager(
            DefaultConnectorFacadeOptions{
                .transport = *this->connectorTransport2,
                .createLocalPeerDescriptor =
                    [noWsServerDescriptor](
                        const ConnectivityResponse& /* response */)
                    -> PeerDescriptor { return noWsServerDescriptor; }});

        const auto biggerNoWsServerDescriptor =
            this->biggerNoWsServerConnectorPeerDescriptor;
        this->biggerNoWsServerManager = createConnectionManager(
            DefaultConnectorFacadeOptions{
                .transport = *this->connectorTransport3,
                .createLocalPeerDescriptor =
                    [biggerNoWsServerDescriptor](
                        const ConnectivityResponse& /* response */)
                    -> PeerDescriptor { return biggerNoWsServerDescriptor; }});

        this->wsServerManager->start();
        this->noWsServerManager->start();
        this->biggerNoWsServerManager->start();
    }

    void TearDown() override {
        this->wsServerManager->stop();
        this->noWsServerManager->stop();
        this->biggerNoWsServerManager->stop();
        this->connectorTransport1->stop();
        this->connectorTransport2->stop();
        this->connectorTransport3->stop();
    }
};

TEST_F(
    WebsocketConnectionManagementTest,
    CanOpenConnectionsToServerlessPeerWithSmallerNodeId) {
    const auto dummyMessage =
        createDummyMessage(this->noWsServerConnectorPeerDescriptor);
    std::atomic<bool> received = false;
    this->noWsServerManager->on<transportevents::Message>(
        [&received](const Message& message) {
            EXPECT_EQ(message.messageid(), "mockerer");
            received = true;
        });
    this->wsServerManager->send(dummyMessage, {});
    expectCondition(
        "message received", [&received]() { return received.load(); });
}

TEST_F(
    WebsocketConnectionManagementTest,
    CanOpenConnectionsToServerlessPeerWithBiggerNodeId) {
    const auto dummyMessage =
        createDummyMessage(this->biggerNoWsServerConnectorPeerDescriptor);
    std::atomic<bool> received = false;
    this->biggerNoWsServerManager->on<transportevents::Message>(
        [&received](const Message& message) {
            EXPECT_EQ(message.messageid(), "mockerer");
            received = true;
        });
    this->wsServerManager->send(dummyMessage, {});
    expectCondition(
        "message received", [&received]() { return received.load(); });
}

TEST_F(
    WebsocketConnectionManagementTest, FailedConnectionRequestsAreCleanedUp) {
    const auto targetDescriptor = createMockPeerDescriptor();
    const auto dummyMessage = createDummyMessage(targetDescriptor);
    const auto nodeId =
        Identifiers::getNodeIdFromPeerDescriptor(targetDescriptor);
    // The TS send() promise rejects when the pending connection fails;
    // the C++ send() buffers into the connecting endpoint and returns.
    this->wsServerManager->send(dummyMessage, {});
    auto* manager = this->wsServerManager.get();
    expectCondition(
        "failed connection request cleaned up",
        [manager, nodeId]() { return !manager->hasConnection(nodeId); },
        failedRequestCleanupTimeout);
}

TEST_F(WebsocketConnectionManagementTest, CanOpenConnectionsToPeerWithServer) {
    const auto dummyMessage =
        createDummyMessage(this->wsServerConnectorPeerDescriptor);
    this->noWsServerManager->send(dummyMessage, {});
    const auto noWsServerNodeId = Identifiers::getNodeIdFromPeerDescriptor(
        this->noWsServerConnectorPeerDescriptor);
    const auto wsServerNodeId = Identifiers::getNodeIdFromPeerDescriptor(
        this->wsServerConnectorPeerDescriptor);
    auto* serverManager = this->wsServerManager.get();
    auto* serverlessManager = this->noWsServerManager.get();
    expectCondition(
        "server sees connection", [serverManager, noWsServerNodeId]() {
            return serverManager->hasConnection(noWsServerNodeId);
        });
    expectCondition(
        "serverless peer sees connection",
        [serverlessManager, wsServerNodeId]() {
            return serverlessManager->hasConnection(wsServerNodeId);
        });
}

TEST_F(WebsocketConnectionManagementTest, ConnectingToSelfThrows) {
    const auto selfMessage1 =
        createDummyMessage(this->noWsServerConnectorPeerDescriptor);
    EXPECT_THROW(
        this->noWsServerManager->send(selfMessage1, {}), CannotConnectToSelf);

    const auto selfMessage2 =
        createDummyMessage(this->wsServerConnectorPeerDescriptor);
    EXPECT_THROW(
        this->wsServerManager->send(selfMessage2, {}), CannotConnectToSelf);
}
