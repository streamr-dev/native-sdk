#include <memory>
#include <string>
#include <gtest/gtest.h>
#include <rtc/rtc.hpp>
#include <folly/experimental/coro/BlockingWait.h>
#include <folly/experimental/coro/Promise.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/ConnectionManager.hpp"
#include "streamr-dht/transport/FakeTransport.hpp"
#include "streamr-dht/types/PortRange.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/collect.hpp"
#include "streamr-utils/waitForCondition.hpp"

using ::dht::ConnectivityResponse;
// using ::dht::Message;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
// using ::protorpc::RpcMessage;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::FakeTransport;
// using streamr::dht::transport::SendOptions;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::LockID;
using streamr::dht::types::PortRange;
using streamr::logger::SLogger;
using streamr::utils::collect;
using streamr::utils::waitForCondition;

// namespace transportevents = streamr::dht::transport::transportevents;

PeerDescriptor createMockPeerDescriptor(uint16_t websocketPort) {
    PeerDescriptor peerDescriptor;
    peerDescriptor.set_nodeid(std::to_string(websocketPort)/*""+Identifiers::getRawFromDhtAddress(
        Identifiers::createRandomDhtAddress())*/ );
    peerDescriptor.set_type(NodeType::NODEJS);
    peerDescriptor.mutable_websocket()->set_port(websocketPort);
    peerDescriptor.mutable_websocket()->set_host(std::string("127.0.0.1"));
    peerDescriptor.mutable_websocket()->set_tls(false);
    peerDescriptor.set_publickey("");
    peerDescriptor.set_signature("");
    peerDescriptor.set_region(1);
    peerDescriptor.set_ipaddress(0);
    return peerDescriptor;
}

std::shared_ptr<ConnectionManager> createConnectionManager(
    DefaultConnectorFacadeOptions options) {
    SLogger::info("Calling connection manager constructor");

    ConnectionManagerOptions connectionManagerOptions{
        .createConnectorFacade = [opts = std::move(options)]()
            -> std::shared_ptr<DefaultConnectorFacade> {
            return std::make_shared<DefaultConnectorFacade>(opts);
        }};
    return std::make_shared<ConnectionManager>(
        std::move(connectionManagerOptions));
}

constexpr auto SERVICE_ID = "demo"; // NOLINT
constexpr uint16_t mockWebsocketPort1 = 9993;
constexpr uint16_t mockWebsocketPort2 = 9994;
constexpr uint16_t mockWebsocketPort3 = 9995;
constexpr uint16_t mockWebsocketPort4 = 9996;

class ConnectionLockingTest : public ::testing::Test {
protected:
    // NOLINTBEGIN
    PeerDescriptor mockPeerDescriptor1 =
        createMockPeerDescriptor(mockWebsocketPort1);
    PeerDescriptor mockPeerDescriptor2 =
        createMockPeerDescriptor(mockWebsocketPort2);
    PeerDescriptor mockPeerDescriptor3 =
        createMockPeerDescriptor(mockWebsocketPort3);
    PeerDescriptor mockPeerDescriptor4 =
        createMockPeerDescriptor(mockWebsocketPort4);

    FakeEnvironment fakeEnvironment;
    std::shared_ptr<FakeTransport> mockConnectorTransport1;
    std::shared_ptr<FakeTransport> mockConnectorTransport2;
    std::shared_ptr<FakeTransport> mockConnectorTransport3;
    std::shared_ptr<FakeTransport> mockConnectorTransport4;

    // NOLINTEND

    void SetUp() override {}

    void TearDown() override {}

public:
    ConnectionLockingTest()
        : mockConnectorTransport1(
              fakeEnvironment.createTransport(mockPeerDescriptor1)),
          mockConnectorTransport2(
              fakeEnvironment.createTransport(mockPeerDescriptor2)),
          mockConnectorTransport3(
              fakeEnvironment.createTransport(mockPeerDescriptor3)),
          mockConnectorTransport4(
              fakeEnvironment.createTransport(mockPeerDescriptor4)) {}
};

TEST_F(ConnectionLockingTest, CanLockConnections) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    SLogger::trace("In the beginning");
    auto connectionManager1 =
        createConnectionManager(DefaultConnectorFacadeOptions{
            .transport = *mockConnectorTransport1,
            .websocketHost = "127.0.0.1",
            .websocketPortRange =
                PortRange{.min = mockWebsocketPort1, .max = mockWebsocketPort1},
            .createLocalPeerDescriptor =
                [this](const ConnectivityResponse& /* response */)
                -> PeerDescriptor { return mockPeerDescriptor1; }});
    SLogger::info("Starting connection manager 1");
    connectionManager1->start();

    auto connectionManager2 =
        createConnectionManager(DefaultConnectorFacadeOptions{
            .transport = *mockConnectorTransport2,
            .websocketHost = "127.0.0.1",
            .websocketPortRange =
                PortRange{.min = mockWebsocketPort2, .max = mockWebsocketPort2},
            .createLocalPeerDescriptor =
                [this](const ConnectivityResponse& /* response */)
                -> PeerDescriptor { return mockPeerDescriptor2; }});

    SLogger::info("Starting connection manager 2");
    connectionManager2->start();
    auto tmpMockPeerDescriptor2 = mockPeerDescriptor2;
    auto tmpMockPeerDescriptor1 = mockPeerDescriptor1;
    SLogger::trace("before lockConnection() start");
    connectionManager1->lockConnection(mockPeerDescriptor2, LockID("testLock"));
    SLogger::trace("lockConnection done");

    std::function<bool()> condition = [&connectionManager2,
                                       &tmpMockPeerDescriptor1]() {
        return connectionManager2->hasRemoteLockedConnection(
            Identifiers::getNodeIdFromPeerDescriptor(tmpMockPeerDescriptor1));
    };
    auto task = waitForCondition(std::move(condition));
    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    ASSERT_TRUE(connectionManager1->hasConnection(
        Identifiers::getNodeIdFromPeerDescriptor(tmpMockPeerDescriptor2)));
    ASSERT_TRUE(connectionManager1->hasLocalLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(tmpMockPeerDescriptor2)));
    ASSERT_TRUE(connectionManager2->hasRemoteLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(tmpMockPeerDescriptor1)));
}

TEST_F(ConnectionLockingTest, LockingBothWays) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    SLogger::trace("In the beginning");
    auto connectionManager3 =
        createConnectionManager(DefaultConnectorFacadeOptions{
            .transport = *mockConnectorTransport3,
            .websocketHost = "127.0.0.1",
            .websocketPortRange =
                PortRange{
                    .min = mockWebsocketPort3,
                    .max = mockWebsocketPort3}, // NOLINT
            .createLocalPeerDescriptor =
                [this](const ConnectivityResponse& /* response */)
                -> PeerDescriptor { return mockPeerDescriptor3; }});
    SLogger::info("Starting connection manager 3");
    connectionManager3->start();

    auto connectionManager4 =
        createConnectionManager(DefaultConnectorFacadeOptions{
            .transport = *mockConnectorTransport4,
            .websocketHost = "127.0.0.1",
            .websocketPortRange =
                PortRange{
                    .min = mockWebsocketPort4,
                    .max = mockWebsocketPort4}, // NOLINT
            .createLocalPeerDescriptor =
                [this](const ConnectivityResponse& /* response */)
                -> PeerDescriptor { return mockPeerDescriptor4; }});

    SLogger::info("Starting connection manager 4");
    connectionManager4->start();

    std::function<bool()> condition = [this,
                                       &connectionManager4,
                                       mockPeerDescriptor3 =
                                           this->mockPeerDescriptor3]() {
        return connectionManager4->hasRemoteLockedConnection(
            Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor3));
    };

    auto task = collect(
        [&]() {
            connectionManager3->lockConnection(
                mockPeerDescriptor4, LockID("testLock1"));
        },
        waitForCondition(condition) // NOLINT
    );

    folly::coro::blockingWait(std::move(task));

    auto task2 = collect(
        [&]() {
            connectionManager4->lockConnection(
                mockPeerDescriptor3, LockID("testLock2"));
        },
        waitForCondition(condition) // NOLINT
    );

    folly::coro::blockingWait(std::move(task2));

    ASSERT_TRUE(connectionManager3->hasConnection(
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor4)));
    ASSERT_TRUE(connectionManager3->hasLocalLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor4)));
    ASSERT_TRUE(connectionManager3->hasRemoteLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor4)));
    ASSERT_TRUE(connectionManager4->hasRemoteLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor3)));
    ASSERT_TRUE(connectionManager4->hasLocalLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor3)));
}
