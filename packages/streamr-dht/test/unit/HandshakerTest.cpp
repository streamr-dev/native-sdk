#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "streamr-dht/connection/Handshaker.hpp"
#include "streamr-dht/connection/OutgoingHandshaker.hpp"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-dht/connection/Connection.hpp"
//#include "streamr-dht/proto/DhtRpc.pb.h"
#include <memory>

//using namespace streamr::dht::connection;
using streamr::dht::connection::OutgoingHandshaker;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::Connection;
using streamr::dht::connection::ConnectionType;
using streamr::dht::connection::Handshaker;
using streamr::dht::connection::connectionevents::Connected;
using ::dht::HandshakeError;
using ::dht::Message;
using ::dht::PeerDescriptor;

class MockPendingConnection : public IPendingConnection {
public:
    MockPendingConnection() = default; 
    MOCK_METHOD(void, close, (bool), (override));
    MOCK_METHOD(void, destroy, (), (override));
    MOCK_METHOD(void, onHandshakeCompleted, (std::shared_ptr<Connection>), (override));
    MOCK_METHOD(const PeerDescriptor&, getPeerDescriptor, (), (const, override));
};

class MockConnection : public Connection {
public:
    MockConnection() : Connection(ConnectionType::WEBSOCKET_CLIENT) {}  // Add this line
    MOCK_METHOD(void, send, (const std::vector<std::byte>&), (override));
    MOCK_METHOD(void, close, (bool), (override));
    MOCK_METHOD(void, destroy, (), (override));
};

class HandshakerTest : public ::testing::Test {
protected:
    std::shared_ptr<MockPendingConnection> pendingConnection;
    std::shared_ptr<MockConnection> connection;
    std::shared_ptr<Handshaker> handshaker;

    void SetUp() override {
        pendingConnection = std::make_shared<MockPendingConnection>();
        connection = std::make_shared<MockConnection>();
    }

    PeerDescriptor createMockPeerDescriptor() {
        PeerDescriptor descriptor;
        descriptor.set_nodeid("test_node_id");
        return descriptor;
    }
};

TEST_F(HandshakerTest, OutgoingSendsRequestAfterConnected) {

   auto localPeerDescriptor = createMockPeerDescriptor();
   auto remotePeerDescriptor = createMockPeerDescriptor();


    handshaker = OutgoingHandshaker::newInstance(
        localPeerDescriptor, 
        connection,
        remotePeerDescriptor,
        pendingConnection
        );

    EXPECT_CALL(*connection, send(::testing::_)).Times(1);
    connection->emit<Connected>();
}

TEST_F(HandshakerTest, OutgoingOnHandshakeCompleted) {
    /*
    handshaker = createOutgoingHandshaker(
        createMockPeerDescriptor(),
        pendingConnection,
        connection,
        createMockPeerDescriptor()
    );
*/
/*
    auto message = createHandshakeResponse(createMockPeerDescriptor());
    EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_)).Times(1);

    connection->emit<connectionevents::Data>(Message::SerializeAsString(message));
    handshaker->emit<handshakerevents::HandshakeCompleted>(createMockPeerDescriptor());
    */
}


TEST_F(HandshakerTest, OutgoingOnHandshakeFailedInvalidPeerDescriptor) {
    /*
    handshaker = createOutgoingHandshaker(
        createMockPeerDescriptor(),
        pendingConnection,
        connection,
        createMockPeerDescriptor()
    );
*/
 //   EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_)).Times(0);
 //   handshaker->emit<handshakerevents::HandshakeFailed>(HandshakeError::INVALID_TARGET_PEER_DESCRIPTOR);
}

TEST_F(HandshakerTest, OutgoingOnHandshakeFailedUnsupportedVersion) {
    /*
    handshaker = createOutgoingHandshaker(
        createMockPeerDescriptor(),
        pendingConnection,
        connection,
        createMockPeerDescriptor()
    );
*/
 //   EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_)).Times(0);
 //   EXPECT_CALL(*pendingConnection, close(::testing::_)).Times(1);
//    handshaker->emit<handshakerevents::HandshakeFailed>(HandshakeError::UNSUPPORTED_VERSION);
}

TEST_F(HandshakerTest, OutgoingOnHandshakeFailedDuplicateConnection) {
    /*
    handshaker = createOutgoingHandshaker(
        createMockPeerDescriptor(),
        pendingConnection,
        connection,
        createMockPeerDescriptor()
    );
*/
  //  EXPECT_CALL(*pendingConnection, destroy()).Times(0);
  //  EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_)).Times(0);
 //   handshaker->emit<handshakerevents::HandshakeFailed>(HandshakeError::DUPLICATE_CONNECTION);
}


TEST_F(HandshakerTest, OutgoingCallsPendingConnectionCloseIfConnectionCloses) {
    /*
    handshaker = createOutgoingHandshaker(
        createMockPeerDescriptor(),
        pendingConnection,
        connection,
        createMockPeerDescriptor()
    );
*/
 //   EXPECT_CALL(*pendingConnection, close(::testing::_)).Times(1);
  //  connection->emit<connectionevents::Disconnected>(true);
}

TEST_F(HandshakerTest, OutgoingClosesConnectionIfManagedConnectionCloses) {
    /*
    handshaker = createOutgoingHandshaker(
        createMockPeerDescriptor(),
        pendingConnection,
        connection,
        createMockPeerDescriptor()
    );
*/
 //   EXPECT_CALL(*connection, close(::testing::_)).Times(1);
 //   pendingConnection->emit<pendingconnectionevents::Disconnected>(true);
}


TEST_F(HandshakerTest, IncomingOnHandshakeRequest) {
    /*
    handshaker = createIncomingHandshaker(createMockPeerDescriptor(), pendingConnection, connection);
*/
  //  auto message = createHandshakeRequest(createMockPeerDescriptor(), createMockPeerDescriptor());
  //  connection->emit<connectionevents::Data>(Message::SerializeAsString(message));
   // handshaker->emit<handshakerevents::HandshakeRequest>(createMockPeerDescriptor(), "1.0");
}


TEST_F(HandshakerTest, IncomingCallsPendingConnectionCloseIfConnectionCloses) {
    /*
    handshaker = createIncomingHandshaker(createMockPeerDescriptor(), pendingConnection, connection);
*/
 //   EXPECT_CALL(*pendingConnection, close(::testing::_)).Times(1);
 //   connection->emit<connectionevents::Disconnected>(true);
}
/*
TEST_F(HandshakerTest, IncomingClosesConnectionIfManagedConnectionCloses) {
    handshaker = createIncomingHandshaker(createMockPeerDescriptor(), pendingConnection, connection);

    EXPECT_CALL(*connection, close(::testing::_)).Times(1);
    pendingConnection->emit<pendingconnectionevents::Disconnected>(true);
}


TEST_F(HandshakerTest, IncomingDestroysConnectionIfHandshakeIsRejected) {
    handshaker = createIncomingHandshaker(createMockPeerDescriptor(), pendingConnection, connection);

    EXPECT_CALL(*pendingConnection, destroy()).Times(1);
    EXPECT_CALL(*connection, destroy()).Times(1);
    rejectHandshake(pendingConnection, connection, handshaker, HandshakeError::DUPLICATE_CONNECTION);
}

TEST_F(HandshakerTest, IncomingCallsOnHandshakeCompletedIfHandshakeIsAccepted) {
    handshaker = createIncomingHandshaker(createMockPeerDescriptor(), pendingConnection, connection);

    EXPECT_CALL(*pendingConnection, onHandshakeCompleted(::testing::_)).Times(1);
    acceptHandshake(handshaker, pendingConnection, connection);
}


*/








