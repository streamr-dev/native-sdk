#include "streamr-dht/connection/PendingConnection.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <folly/coro/BlockingWait.h>
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-utils/waitForCondition.hpp"

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
