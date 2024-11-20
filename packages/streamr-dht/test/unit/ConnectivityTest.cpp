#include "streamr-dht/helpers/Connectivity.hpp"
#include <gtest/gtest.h>

using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectionType;
using streamr::dht::helpers::Connectivity;

class ConnectivityTest : public ::testing::Test {
protected:
    PeerDescriptor tlsServerPeerDescriptor =
        createPeerDescriptor(NodeType::NODEJS, true, true);
    PeerDescriptor noTlsServerPeerDescriptor =
        createPeerDescriptor(NodeType::NODEJS, true, false);
    PeerDescriptor browserPeerDescriptor =
        createPeerDescriptor(NodeType::BROWSER, false, false);
    PeerDescriptor noServerPeerDescriptor =
        createPeerDescriptor(NodeType::NODEJS, false, false);

    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Teardown code if needed
    }

    // Helper function to create a PeerDescriptor
    static PeerDescriptor createPeerDescriptor(
        NodeType type, bool hasWebsocket, bool isTls) {
        PeerDescriptor descriptor;
        descriptor.set_type(type);
        if (hasWebsocket) {
            descriptor.mutable_websocket()->set_host("example.com");
            descriptor.mutable_websocket()->set_tls(isTls);
        }
        return descriptor;
    }
};

TEST_F(ConnectivityTest, ExpectedConnectionType_TwoServerPeers) { // NOLINT
    EXPECT_EQ(
        Connectivity::expectedConnectionType(
            tlsServerPeerDescriptor, tlsServerPeerDescriptor),
        ConnectionType::WEBSOCKET_CLIENT);
}

TEST_F(ConnectivityTest, ExpectedConnectionType_ServerToNoServer) { // NOLINT
    EXPECT_EQ(
        Connectivity::expectedConnectionType(
            tlsServerPeerDescriptor, noServerPeerDescriptor),
        ConnectionType::WEBSOCKET_SERVER);
}

TEST_F(ConnectivityTest, ExpectedConnectionType_NoServerToServer) { // NOLINT
    EXPECT_EQ(
        Connectivity::expectedConnectionType(
            noServerPeerDescriptor, tlsServerPeerDescriptor),
        ConnectionType::WEBSOCKET_CLIENT);
}

TEST_F(ConnectivityTest, ExpectedConnectionType_NoServerToNoServer) { // NOLINT
    EXPECT_EQ(
        Connectivity::expectedConnectionType(
            noServerPeerDescriptor, noServerPeerDescriptor),
        ConnectionType::WEBRTC);
}

TEST_F(ConnectivityTest, ExpectedConnectionType_BrowserToTlsServer) { // NOLINT
    EXPECT_EQ(
        Connectivity::expectedConnectionType(
            browserPeerDescriptor, tlsServerPeerDescriptor),
        ConnectionType::WEBSOCKET_CLIENT);
}

TEST_F(ConnectivityTest, ExpectedConnectionType_TlsServerToBrowser) { // NOLINT
    EXPECT_EQ(
        Connectivity::expectedConnectionType(
            tlsServerPeerDescriptor, browserPeerDescriptor),
        ConnectionType::WEBSOCKET_SERVER);
}

TEST_F(
    ConnectivityTest, ExpectedConnectionType_BrowserToNoTlsserver) { // NOLINT
    EXPECT_EQ(
        Connectivity::expectedConnectionType(
            browserPeerDescriptor, noTlsServerPeerDescriptor),
        ConnectionType::WEBRTC);
}

TEST_F(
    ConnectivityTest, ExpectedConnectionType_NoTlsServerToBrowser) { // NOLINT
    EXPECT_EQ(
        Connectivity::expectedConnectionType(
            noTlsServerPeerDescriptor, browserPeerDescriptor),
        ConnectionType::WEBRTC);
}
