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
    const DefaultConnectorFacadeOptions& opts) {
    SLogger::info("Calling connection manager constructor");

    ConnectionManagerOptions connectionManagerOptions{
        .createConnectorFacade =
            [&opts]() -> std::shared_ptr<DefaultConnectorFacade> {
            return std::make_shared<DefaultConnectorFacade>(opts);
        }};
    return std::make_shared<ConnectionManager>(
        std::move(connectionManagerOptions));
}

constexpr auto SERVICE_ID = "demo"; // NOLINT
constexpr uint16_t mockWebsocketPort1 = 9995;
constexpr uint16_t mockWebsocketPort2 = 9996;

class ConnectionLockingTest : public ::testing::Test {
protected:
    // NOLINTBEGIN
    PeerDescriptor mockPeerDescriptor1 =
        createMockPeerDescriptor(mockWebsocketPort1);
    PeerDescriptor mockPeerDescriptor2 =
        createMockPeerDescriptor(mockWebsocketPort2);

    FakeEnvironment fakeEnvironment;
    std::shared_ptr<FakeTransport> mockConnectorTransport1;
    std::shared_ptr<FakeTransport> mockConnectorTransport2;

    // NOLINTEND

    void SetUp() override {}

    void TearDown() override {}

public:
    ConnectionLockingTest()
        : mockConnectorTransport1(
              fakeEnvironment.createTransport(mockPeerDescriptor1)),
          mockConnectorTransport2(
              fakeEnvironment.createTransport(mockPeerDescriptor2)) {}
};

TEST_F(ConnectionLockingTest, DISABLED_CanLockConnections) {
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
    auto task = waitForCondition(condition);
    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    ASSERT_TRUE(connectionManager1->hasConnection(
        Identifiers::getNodeIdFromPeerDescriptor(tmpMockPeerDescriptor2)));
    ASSERT_TRUE(connectionManager1->hasLocalLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(tmpMockPeerDescriptor2)));
    ASSERT_TRUE(connectionManager2->hasRemoteLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(tmpMockPeerDescriptor1)));
}

TEST_F(ConnectionLockingTest, DISABLED_MultipleServicesOnTheSamePeer) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    SLogger::trace("In the beginning");
    auto connectionManager1 =
        createConnectionManager(DefaultConnectorFacadeOptions{
            .transport = *mockConnectorTransport1,
            .websocketHost = "127.0.0.1",
            .websocketPortRange =
                PortRange{
                    .min = mockWebsocketPort1,
                    .max = mockWebsocketPort1}, // NOLINT
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
                PortRange{
                    .min = mockWebsocketPort2,
                    .max = mockWebsocketPort2}, // NOLINT
            .createLocalPeerDescriptor =
                [this](const ConnectivityResponse& /* response */)
                -> PeerDescriptor { return mockPeerDescriptor2; }});

    SLogger::info("Starting connection manager 2");
    connectionManager2->start();

    std::function<bool()> condition = [this,
                                       &connectionManager2,
                                       mockPeereerDescriptor1 =
                                           this->mockPeerDescriptor1]() {
        return connectionManager2->hasRemoteLockedConnection(
            Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor1));
    };

    auto task = collect(
        [&]() {
            connectionManager1->lockConnection(
                mockPeerDescriptor2, LockID("testLock1"));
        },
        waitForCondition(condition) // NOLINT
    );

    folly::coro::blockingWait(std::move(task));

    auto task2 = collect(
        [&]() {
            connectionManager1->lockConnection(
                mockPeerDescriptor2, LockID("testLock2"));
        },
        waitForCondition(condition) // NOLINT
    );

    folly::coro::blockingWait(std::move(task2));

    ASSERT_TRUE(connectionManager1->hasConnection(
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor2)));
    ASSERT_TRUE(connectionManager1->hasLocalLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor2)));
    ASSERT_TRUE(connectionManager2->hasRemoteLockedConnection(
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor1)));
}
