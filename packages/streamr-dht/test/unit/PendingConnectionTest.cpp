#include <chrono>
#include <gtest/gtest.h>
// Same include path as the module GMFs (the experimental shim): mixing the
// folly/coro/... and folly/experimental/coro/... spellings of this header
// between an importing TU and the imported BMIs makes clang see duplicate
// (unmergeable) definitions with identical mangled names in Release.
#include <folly/experimental/coro/BlockingWait.h>

import streamr.dht.IPendingConnection;
import streamr.dht.Connection;
import streamr.dht.PendingConnection;
import streamr.dht.Transport;
import streamr.dht.protos;
import streamr.utils;

using ::dht::PeerDescriptor;
using streamr::dht::connection::Connection;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::pendingconnectionevents::Connected;
using streamr::dht::connection::pendingconnectionevents::Disconnected;
using streamr::utils::waitForCondition;
using namespace std::chrono_literals;

class DummyConnection : public streamr::dht::connection::Connection {
public:
    DummyConnection()
        : Connection(
              streamr::dht::connection::ConnectionType::WEBSOCKET_CLIENT) {}

    void send(const std::vector<std::byte>& data) override {}

    void close(bool gracefulLeave) override {}

    void destroy() override {}
};

class PendingConnectionTest : public ::testing::Test {
protected:
    PeerDescriptor mockPeerDescriptor;
    std::unique_ptr<PendingConnection> pendingConnection;
    static constexpr auto timeout = std::chrono::seconds(5);
    static constexpr auto retryInternal = std::chrono::milliseconds(100);

    void SetUp() override {
        mockPeerDescriptor = createMockPeerDescriptor();
        pendingConnection = std::make_unique<PendingConnection>(
            mockPeerDescriptor,
            std::nullopt,
            std::chrono::milliseconds(500)); // NOLINT
    }

    void TearDown() override { pendingConnection->close(false); }

    static PeerDescriptor createMockPeerDescriptor() {
        PeerDescriptor descriptor;
        descriptor.set_nodeid("test_nodeid");
        return descriptor;
    }
};

TEST_F(PendingConnectionTest, DoesNotEmitDisconnectedAfterReplacedAsDuplicate) {
    bool isEmitted = false;
    pendingConnection->once<Disconnected>(
        [&](bool /* gracefulLeave */) { isEmitted = true; });
    pendingConnection->replaceAsDuplicate();
    pendingConnection->close(false);
    EXPECT_FALSE(isEmitted);
}

TEST_F(PendingConnectionTest, EmitsDisconnectedAfterTimedOut) {
    bool isEmitted = false;
    std::function<bool()> condition = [&isEmitted]() { return isEmitted; };
    pendingConnection->once<Disconnected>(
        [&](bool /* gracefulLeave */) { isEmitted = true; });

    auto task = waitForCondition(std::move(condition), timeout, retryInternal);
    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    EXPECT_TRUE(isEmitted);
}

TEST_F(PendingConnectionTest, DoesNotEmitDisconnectedIfDestroyed) {
    bool isEmitted = false;
    pendingConnection->once<Disconnected>(
        [&](bool /* gracefulLeave */) { isEmitted = true; });
    pendingConnection->replaceAsDuplicate();
    pendingConnection->destroy();
    EXPECT_FALSE(isEmitted);
}

TEST_F(PendingConnectionTest, EmitsConnected) {
    auto dummyConnection = std::make_shared<DummyConnection>();
    pendingConnection->once<Connected>(
        [&](const PeerDescriptor& /* peerDescriptor */,
            std::shared_ptr<Connection> connection) { // NOLINT
            EXPECT_EQ(dummyConnection, connection);
        });
    pendingConnection->onHandshakeCompleted(dummyConnection);
}

// Regression tests for the CanLockConnections stall on slow CI runners:
// the connectors start socket I/O before ConnectionManager registers the
// endpoint's listeners on the pending connection, so a fast handshake can
// complete before registration. IPendingConnection must therefore replay
// the latest emission to late listeners.

TEST_F(PendingConnectionTest, ReplaysConnectedToLateListener) {
    auto dummyConnection = std::make_shared<DummyConnection>();
    // handshake completes before anyone listens
    pendingConnection->onHandshakeCompleted(dummyConnection);
    bool isEmitted = false;
    pendingConnection->once<Connected>(
        [&](const PeerDescriptor& /* peerDescriptor */,
            std::shared_ptr<Connection> connection) { // NOLINT
            isEmitted = true;
            EXPECT_EQ(dummyConnection, connection);
        });
    EXPECT_TRUE(isEmitted);
}

TEST_F(PendingConnectionTest, ReplaysDisconnectedToLateListener) {
    // connection fails before anyone listens
    pendingConnection->close(false);
    bool isEmitted = false;
    pendingConnection->once<Disconnected>(
        [&](bool /* gracefulLeave */) { isEmitted = true; });
    EXPECT_TRUE(isEmitted);
}

TEST_F(PendingConnectionTest, DoesNotEmitConnectedIfReplaced) {
    bool isEmitted = false;
    pendingConnection->once<Connected>(
        [&](const PeerDescriptor& /* peerDescriptor */,
            std::shared_ptr<Connection> /*connection */) { // NOLINT
            isEmitted = true;
        });
    pendingConnection->replaceAsDuplicate();
    EXPECT_FALSE(isEmitted);
}
