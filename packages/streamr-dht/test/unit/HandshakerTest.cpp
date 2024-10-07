#include "streamr-dht/connection/Handshaker.hpp"
#include <memory>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-dht/connection/OutgoingHandshaker.hpp"

using ::dht::HandshakeError;
using ::dht::PeerDescriptor;
using streamr::dht::connection::Connection;
using streamr::dht::connection::ConnectionType;
using streamr::dht::connection::Handshaker;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::OutgoingHandshaker;
using streamr::dht::connection::connectionevents::Connected;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::connection::connectionevents::Disconnected;
using streamr::dht::connection::handshakerevents::HandshakeCompleted;
using streamr::dht::connection::handshakerevents::HandshakeFailed;

class MockPendingConnection : public IPendingConnection {
public:
    MockPendingConnection() = default;
    MOCK_METHOD(void, onHandshakeCompleted, (const std::shared_ptr<Connection>&), (override)); // NOLINT
    MOCK_METHOD(void, close, (bool), (override)); // NOLINT
    MOCK_METHOD(void, destroy, (), (override)); // NOLINT
    MOCK_METHOD(const PeerDescriptor&, getPeerDescriptor, (), (const, override)); // NOLINT
    MOCK_METHOD(void, onError, (const std::exception_ptr&), (override)); // NOLINT
};

class MockConnection : public Connection {
public:
    MockConnection()
        : Connection(ConnectionType::WEBSOCKET_CLIENT) {} // Add this line
    MOCK_METHOD(void, send, (const std::vector<std::byte>&), (override)); // NOLINT
    MOCK_METHOD(void, close, (bool), (override)); // NOLINT
    MOCK_METHOD(void, destroy, (), (override)); // NOLINT
};

class HandshakerTest : public ::testing::Test {
protected:
    std::shared_ptr<MockPendingConnection> pendingConnection;
    std::shared_ptr<MockConnection> connection;
    std::shared_ptr<Handshaker> handshaker;
    PeerDescriptor localPeerDescriptor;
    PeerDescriptor remotePeerDescriptor;
    
    void SetUp() override {
        pendingConnection = std::make_shared<MockPendingConnection>();
        connection = std::make_shared<MockConnection>();
        localPeerDescriptor = createMockPeerDescriptor();
        remotePeerDescriptor = createMockPeerDescriptor();
        handshaker = OutgoingHandshaker::newInstance(
            localPeerDescriptor,
            connection,
            remotePeerDescriptor,
            pendingConnection);
    }

    static PeerDescriptor createMockPeerDescriptor() {
        PeerDescriptor descriptor;
        descriptor.set_nodeid("test_node_id");
        return descriptor;
    }
};

TEST_F(HandshakerTest, OutgoingSendsRequestAfterConnected) {
    EXPECT_CALL(*connection, send(::testing::_)).Times(1);
    connection->emit<Connected>();
}

TEST_F(HandshakerTest, OutgoingOnHandshakeCompleted) {
    auto message = handshaker->createHandshakeResponse(std::nullopt);
    EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_))
        .Times(1);
    size_t nBytes = message.ByteSizeLong();
    std::vector<std::byte> byteVec(nBytes);
    message.SerializeToArray(byteVec.data(), static_cast<int>(nBytes));
    connection->emit<Data>(byteVec);
    handshaker->emit<HandshakeCompleted>(createMockPeerDescriptor());
}

TEST_F(HandshakerTest, OutgoingOnHandshakeFailedInvalidPeerDescriptor) {
    EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_))
        .Times(0);
    handshaker->emit<HandshakeFailed>(
        HandshakeError::INVALID_TARGET_PEER_DESCRIPTOR);
}

TEST_F(HandshakerTest, OutgoingOnHandshakeFailedUnsupportedVersion) {
    EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_))
        .Times(0);
    EXPECT_CALL(*pendingConnection, close(::testing::_)).Times(1);
    handshaker->emit<HandshakeFailed>(HandshakeError::UNSUPPORTED_VERSION);
}

TEST_F(HandshakerTest, OutgoingOnHandshakeFailedDuplicateConnection) {
    EXPECT_CALL(*pendingConnection, destroy()).Times(0);
    EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_))
        .Times(0);
    handshaker->emit<HandshakeFailed>(HandshakeError::DUPLICATE_CONNECTION);
}

TEST_F(HandshakerTest, OutgoingCallsPendingConnectionCloseIfConnectionCloses) {
    EXPECT_CALL(*pendingConnection, close(::testing::_)).Times(1);
    connection->emit<Disconnected>(true, 0, "");
}

TEST_F(HandshakerTest, OutgoingClosesConnectionIfManagedConnectionCloses) {
    EXPECT_CALL(*connection, close(::testing::_)).Times(1);
    pendingConnection
        ->emit<streamr::dht::connection::pendingconnectionevents::Disconnected>(
            true);
}

TEST_F(HandshakerTest, IncomingCallsPendingConnectionCloseIfConnectionCloses) {
    EXPECT_CALL(*pendingConnection, close(::testing::_)).Times(1);
    connection->emit<Disconnected>(true, 0, "");
}

TEST_F(HandshakerTest, IncomingClosesConnectionIfManagedConnectionCloses) {
    EXPECT_CALL(*connection, close(::testing::_)).Times(1);
    pendingConnection
        ->emit<streamr::dht::connection::pendingconnectionevents::Disconnected>(
            true);
}

/*
TEST_F(HandshakerTest, IncomingDestroysConnectionIfHandshakeIsRejected) {
    EXPECT_CALL(*pendingConnection, destroy()).Times(1);
    EXPECT_CALL(*connection, destroy()).Times(1);
    rejectHandshake(
        pendingConnection,
        connection,
        handshaker,
        HandshakeError::DUPLICATE_CONNECTION);
}
*/

/*
TEST_F(HandshakerTest, IncomingCallsOnHandshakeCompletedIfHandshakeIsAccepted) {
    EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_))
        .Times(1);
    acceptHandshake(handshaker, pendingConnection, connection);
}
*/