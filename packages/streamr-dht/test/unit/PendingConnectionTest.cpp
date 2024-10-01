#include <gtest/gtest.h>
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/connection/Connection.hpp"
#include <chrono>
#include <thread>
#include <streamr-utils/waitForEvent.hpp>
#include <streamr-utils/waitForCondition.hpp>
#include <folly/coro/BlockingWait.h>

using ::dht::PeerDescriptor;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::Connection;
using streamr::dht::connection::pendingconnectionevents::Disconnected;
using streamr::dht::connection::pendingconnectionevents::Connected;
using streamr::utils::waitForEvent;
using streamr::utils::waitForCondition;
using namespace std::chrono_literals;

class DummyConnection : public streamr::dht::connection::Connection {
public:
    DummyConnection() : Connection(streamr::dht::connection::ConnectionType::WEBSOCKET_CLIENT) {}

    void send(const std::vector<std::byte>& data) override {
        std::cout << "DummyConnection: Sending " << data.size() << " bytes" << std::endl;
    }

    void close(bool gracefulLeave) override {
        std::cout << "DummyConnection: Closing (graceful: " << (gracefulLeave ? "true" : "false") << ")" << std::endl;
    }

    void destroy() override {
        std::cout << "DummyConnection: Destroying" << std::endl;
    }
};

class PendingConnectionTest : public ::testing::Test {
protected:
    PeerDescriptor mockPeerDescriptor;
    std::unique_ptr<PendingConnection> pendingConnection;

    void SetUp() override {
        mockPeerDescriptor = createMockPeerDescriptor();
        pendingConnection = std::make_unique<PendingConnection>(mockPeerDescriptor, std::chrono::milliseconds(500));
    }

    void TearDown() override {
        pendingConnection->close(false);
    }

    PeerDescriptor createMockPeerDescriptor() {
        PeerDescriptor descriptor;
        descriptor.set_nodeid("test_nodeid");
        return descriptor;
    }
};

TEST_F(PendingConnectionTest, DoesNotEmitDisconnectedAfterReplacedAsDuplicate) {
    bool isEmitted = false;
    pendingConnection->once<Disconnected>([&](bool gracefulLeave) {
        isEmitted = true;
    });
    pendingConnection->replaceAsDuplicate();
    pendingConnection->close(false);
    EXPECT_FALSE(isEmitted);
}

TEST_F(PendingConnectionTest, EmitsDisconnectedAfterTimedOut) {
    bool isEmitted = false;
    std::function<bool()> condition = [&isEmitted]() {
        return isEmitted;
    };
    pendingConnection->once<Disconnected>([&](bool gracefulLeave) {
        isEmitted = true;
    });

    auto task = waitForCondition(condition, 5s, 100ms);
    EXPECT_NO_THROW(folly::coro::blockingWait(std::move(task)));
    EXPECT_TRUE(isEmitted);
}

TEST_F(PendingConnectionTest, DoesNotEmitDisconnectedIfDestroyed) {
    bool isEmitted = false;
    pendingConnection->once<Disconnected>([&](bool gracefulLeave) {
        isEmitted = true;
    });
    pendingConnection->replaceAsDuplicate();
    pendingConnection->destroy();
    EXPECT_FALSE(isEmitted);
}

TEST_F(PendingConnectionTest, EmitsConnected) {
    auto dummyConnection = std::make_shared<DummyConnection>();
    pendingConnection->once<Connected>([&](PeerDescriptor peerDescriptor, std::shared_ptr<Connection> connection) {
         EXPECT_EQ(dummyConnection, connection);
    });
    pendingConnection->onHandshakeCompleted(dummyConnection);
}

TEST_F(PendingConnectionTest, DoesNotEmitConnectedIfReplaced) {
    bool isEmitted = false;
    pendingConnection->once<Connected>([&](PeerDescriptor peerDescriptor, std::shared_ptr<Connection> connection) {
        isEmitted = true;
    });
    pendingConnection->replaceAsDuplicate();
    EXPECT_FALSE(isEmitted);
}

