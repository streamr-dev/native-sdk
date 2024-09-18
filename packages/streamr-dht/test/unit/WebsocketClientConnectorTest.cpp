#include "streamr-dht/connection/websocket/WebsocketClientConnector.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-proto-rpc/ProtoCallContext.hpp"

using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::pendingconnectionevents::Disconnected;
using streamr::dht::connection::websocket::WebsocketClientConnector;
using streamr::dht::connection::websocket::WebsocketClientConnectorOptions;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::SendOptions;
using streamr::eventemitter::HandlerToken;

class DummyTransport : public streamr::dht::transport::Transport {
public:
    void send(
        const dht::Message& message, const SendOptions& sendOptions) override {}

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const override {
        return PeerDescriptor(); // Return a default-constructed PeerDescriptor
    }

    void stop() override {}
};

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

class WebsocketClientConnectorTest : public ::testing::Test {
protected:
    std::unique_ptr<DummyTransport> dummyTransport;
    std::unique_ptr<ListeningRpcCommunicator> dummyCommunicator;
    std::unique_ptr<WebsocketClientConnector> connector;
    PeerDescriptor localPeerDescriptor;

    void SetUp() override {
        dummyTransport = std::make_unique<DummyTransport>();
        dummyCommunicator = std::make_unique<ListeningRpcCommunicator>(
            streamr::dht::ServiceID("dummy_service"),
            *dummyTransport,
            std::nullopt);
        WebsocketClientConnectorOptions options{
            .onNewConnection =
                [](const std::shared_ptr<PendingConnection>& /*connection*/) {
                    return true;
                },
            .hasConnection = [](DhtAddress) { return false; },
            .rpcCommunicator = *dummyCommunicator};

        // Create the connector
        connector =
            std::make_unique<WebsocketClientConnector>(std::move(options));
    }

    static PeerDescriptor createMockPeerDescriptor(
        const std::string& id,
        bool hasWebsocket,
        bool isTls = false,
        NodeType nodeType = NodeType::NODEJS,
        std::string hostAddress = "localhost") {
        PeerDescriptor pd;

        pd.set_nodeid(id);
        pd.set_type(nodeType);
        if (hasWebsocket) {
            auto* ws = pd.mutable_websocket();
            ws->set_host("localhost");
            ws->set_port(8080);
            ws->set_tls(isTls);
        }
        return pd;
    }
};

TEST_F(
    WebsocketClientConnectorTest, IsPossibleToFormConnection_NodeWithoutServer) {
    connector->setLocalPeerDescriptor(createMockPeerDescriptor("local", false));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true)));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true, true)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false, false, NodeType::BROWSER)));
}

TEST_F(
    WebsocketClientConnectorTest, IsPossibleToFormConnection_NodeWithTLSServer) {
    connector->setLocalPeerDescriptor(
        createMockPeerDescriptor("local", true, true));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true)));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true, true)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false, false, NodeType::BROWSER)));
}

TEST_F(
    WebsocketClientConnectorTest,
    IsPossibleToFormConnection_NodeWithNonTLSServer) {
    connector->setLocalPeerDescriptor(
        createMockPeerDescriptor("local", true, false));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true)));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true, true)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false, false, NodeType::BROWSER)));
}

TEST_F(
    WebsocketClientConnectorTest,
    IsPossibleToFormConnection_NodeWithNonTLSServerInLocalNetwork) {
    connector->setLocalPeerDescriptor(createMockPeerDescriptor(
        "local", true, false, NodeType::NODEJS, "192.168.11.11"));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true)));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true, true)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false, false, NodeType::BROWSER)));
}

TEST_F(WebsocketClientConnectorTest, IsPossibleToFormConnection_Browser) {
    connector->setLocalPeerDescriptor(
        createMockPeerDescriptor("local", false, false, NodeType::BROWSER));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true)));
    EXPECT_TRUE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", true, true)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false)));
    EXPECT_FALSE(connector->isPossibleToFormConnection(
        createMockPeerDescriptor("remote", false, false, NodeType::BROWSER)));
}

TEST_F(WebsocketClientConnectorTest, Connect_ReturnsExistingConnectingConnection) {
    connector->setLocalPeerDescriptor(
        createMockPeerDescriptor("local", false));
    auto remotePeerDescriptor = createMockPeerDescriptor("remote", true);
    auto firstConnection = connector->connect(remotePeerDescriptor);
    auto secondConnection = connector->connect(remotePeerDescriptor);
    EXPECT_TRUE(firstConnection == secondConnection);
    firstConnection->close(false);
}

TEST_F(WebsocketClientConnectorTest, Connect_DisconnectedEventRemovesConnectingConnection) {
    connector->setLocalPeerDescriptor(
        createMockPeerDescriptor("local", false));
    auto remotePeerDescriptor = createMockPeerDescriptor("remote", true);
    auto firstConnection = connector->connect(remotePeerDescriptor);
    firstConnection->emit<Disconnected>(false); 
    auto secondConnection = connector->connect(remotePeerDescriptor);
    EXPECT_FALSE(firstConnection == secondConnection);
    firstConnection->close(false);
    secondConnection->close(false);
}

TEST_F(WebsocketClientConnectorTest, Connect_ConnectedEventRemovesConnectingConnection) {
    connector->setLocalPeerDescriptor(
        createMockPeerDescriptor("local", false));
    auto remotePeerDescriptor = createMockPeerDescriptor("remote", true);
    auto firstConnection = connector->connect(remotePeerDescriptor);
    firstConnection->emit<Disconnected>(false); 
    auto secondConnection = connector->connect(remotePeerDescriptor);
    EXPECT_FALSE(firstConnection == secondConnection);
    firstConnection->close(false);
    secondConnection->close(false);
}