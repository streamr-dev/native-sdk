#include <memory>
#include <string>
#include <gtest/gtest.h>
#include <rtc/rtc.hpp>
#include <folly/coro/Collect.h>
#include <folly/coro/Promise.h>
#include <folly/coro/blockingWait.h>
#include "FakeTransport.hpp"
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/ConnectionManager.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-dht/types/PortRange.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/waitForCondition.hpp"

using ::dht::ConnectivityResponse;
using ::dht::Message;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
using ::protorpc::RpcMessage;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::connection::LockID;
using streamr::dht::test::utils::FakeEnvironment;
using streamr::dht::test::utils::FakeTransport;
using streamr::dht::transport::SendOptions;
using streamr::dht::types::PortRange;
using streamr::logger::SLogger;
using streamr::utils::waitForCondition;

namespace transportevents = streamr::dht::transport::transportevents;

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
    DhtAddress node1 =
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor1);
    DhtAddress node2 =
        Identifiers::getNodeIdFromPeerDescriptor(mockPeerDescriptor2);
    FakeEnvironment fakeEnvironment;
    std::shared_ptr<FakeTransport> mockConnectorTransport1;
    std::shared_ptr<FakeTransport> mockConnectorTransport2;
    std::shared_ptr<ConnectionManager> connectionManager1;
    std::shared_ptr<ConnectionManager> connectionManager2;

    // NOLINTEND
    void SetUp() override {
        connectionManager1 =
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

        connectionManager2 =
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
    }

    void TearDown() override {}

public:
    ConnectionLockingTest()
        : mockConnectorTransport1(
              fakeEnvironment.createTransport(mockPeerDescriptor1)),
          mockConnectorTransport2(
              fakeEnvironment.createTransport(mockPeerDescriptor2)) {}
};

TEST_F(ConnectionLockingTest, CanLockConnections) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    SLogger::trace("In the beginning");
    SLogger::trace("before lockConnection() start");
    connectionManager1->lockConnection(
        mockPeerDescriptor2, std::move(LockID("testLock")));
    SLogger::trace("lockConnection done");
    std::function<bool()> condition = [&]() {
        return connectionManager2->hasRemoteLockedConnection(node1);
    };
    auto task = waitForCondition(condition);
    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    ASSERT_TRUE(connectionManager1->hasConnection(node2));
    ASSERT_TRUE(connectionManager1->hasLocalLockedConnection(node2));
    ASSERT_TRUE(connectionManager2->hasRemoteLockedConnection(node1));
}

TEST_F(ConnectionLockingTest, MultipleServicesOnTheSamePeer) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    SLogger::trace("In the beginning");
    std::function<bool()> condition = [&]() {
        return connectionManager2->hasRemoteLockedConnection(node1);
    };
    auto task = folly::coro::collectAll(
        folly::coro::co_invoke([&]() -> folly::coro::Task<void> {
            connectionManager1->lockConnection(
                mockPeerDescriptor2, LockID("testLock1"));
            co_return;
        }),
        waitForCondition(condition) // NOLINT
    );
    folly::coro::blockingWait(std::move(task));
    auto task2 = folly::coro::collectAll(
        folly::coro::co_invoke([&]() -> folly::coro::Task<void> {
            connectionManager1->lockConnection(
                mockPeerDescriptor2, std::move(LockID("testLock2")));
            co_return;
        }),
        waitForCondition(condition) // NOLINT
    );
    folly::coro::blockingWait(std::move(task2));
    ASSERT_TRUE(connectionManager1->hasConnection(node2));
    ASSERT_TRUE(connectionManager1->hasLocalLockedConnection(node2));
    ASSERT_TRUE(connectionManager2->hasRemoteLockedConnection(node1));
}

TEST_F(ConnectionLockingTest, CanUnlockConnections) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    SLogger::trace("In the beginning");
    SLogger::trace("before lockConnection() start");
    connectionManager1->lockConnection(
        mockPeerDescriptor2, std::move(LockID("testLock")));
    SLogger::trace("lockConnection done");
    std::function<bool()> condition = [&]() {
        return connectionManager2->hasRemoteLockedConnection(node1);
    };
    auto task = waitForCondition(condition);
    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    ASSERT_TRUE(connectionManager1->hasConnection(node2));
    ASSERT_FALSE(connectionManager2->hasLocalLockedConnection(node2));
    ASSERT_TRUE(connectionManager2->hasRemoteLockedConnection(node1));
    connectionManager1->unlockConnection(
        mockPeerDescriptor2, std::move(LockID("testLock")));
    SLogger::trace("unlockConnection done");
    ASSERT_FALSE(connectionManager1->hasLocalLockedConnection(node2));
    auto task2 = waitForCondition(condition);
    folly::coro::blockingWait(std::move(task2));
    ASSERT_FALSE(connectionManager1->hasConnection(node1));
}

TEST_F(ConnectionLockingTest, UnlockingMultipleServices) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    SLogger::trace("In the beginning");
    std::function<bool()> condition = [&]() {
        return connectionManager2->hasRemoteLockedConnection(node1);
    };
    auto task = folly::coro::collectAll(
        folly::coro::co_invoke([&]() -> folly::coro::Task<void> {
            connectionManager1->lockConnection(
                mockPeerDescriptor2, LockID("testLock1"));
            co_return;
        }),
        waitForCondition(condition) // NOLINT
    );
    folly::coro::blockingWait(std::move(task));
    auto task2 = folly::coro::collectAll(
        folly::coro::co_invoke([&]() -> folly::coro::Task<void> {
            connectionManager1->lockConnection(
                mockPeerDescriptor2, std::move(LockID("testLock2")));
            co_return;
        }),
        waitForCondition(condition) // NOLINT
    );
    folly::coro::blockingWait(std::move(task2));
    ASSERT_TRUE(connectionManager1->hasConnection(node2));
    ASSERT_FALSE(connectionManager2->hasLocalLockedConnection(node1));
    connectionManager1->unlockConnection(
        mockPeerDescriptor2, std::move(LockID("testLock1")));
    ASSERT_TRUE(connectionManager1->hasLocalLockedConnection(node2));
    connectionManager1->unlockConnection(
        mockPeerDescriptor2, std::move(LockID("testLock2")));
    ASSERT_FALSE(connectionManager1->hasLocalLockedConnection(node2));
    ASSERT_FALSE(connectionManager1->hasConnection(node1));
}

TEST_F(
    ConnectionLockingTest,
    MaintainsConnectionIfBothSidesInitiallyLockAndThenOneEndUnlocks) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    SLogger::trace("In the beginning");
    std::function<bool()> condition1 = [&]() {
        return connectionManager2->hasRemoteLockedConnection(node1);
    };
    std::function<bool()> condition2 = [&]() {
        return connectionManager1->hasRemoteLockedConnection(node2);
    };
    auto task = folly::coro::collectAll(
        folly::coro::co_invoke([&]() -> folly::coro::Task<void> {
            connectionManager1->lockConnection(
                mockPeerDescriptor2, LockID("testLock1"));
            co_return;
        }),
        folly::coro::co_invoke([&]() -> folly::coro::Task<void> {
            connectionManager2->lockConnection(
                mockPeerDescriptor1, LockID("testLock1"));
            co_return;
        }),
        waitForCondition(condition1), // NOLINT
        waitForCondition(condition2));
    folly::coro::blockingWait(std::move(task));
    ASSERT_TRUE(connectionManager1->hasLocalLockedConnection(node2));
    ASSERT_TRUE(connectionManager2->hasLocalLockedConnection(node1));
    connectionManager1->unlockConnection(
        mockPeerDescriptor2, std::move(LockID("testLock1")));
    std::function<bool()> condition3 = [&]() {
        return !connectionManager1->hasLocalLockedConnection(node2);
    };
    std::function<bool()> condition4 = [&]() {
        return !connectionManager2->hasRemoteLockedConnection(node1);
    };
    std::function<bool()> condition5 = [&]() {
        return connectionManager2->hasLocalLockedConnection(node1);
    };
    auto task2 = folly::coro::collectAll(
        waitForCondition(condition2),
        waitForCondition(condition4),
        waitForCondition(condition4),
        waitForCondition(condition5));
    folly::coro::blockingWait(std::move(task2));
    ASSERT_TRUE(connectionManager2->hasConnection(node1));
    ASSERT_TRUE(connectionManager1->hasConnection(node2));
}
