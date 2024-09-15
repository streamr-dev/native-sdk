#include "streamr-dht/connection/websocket/WebsocketClientConnector.hpp"
#include <gtest/gtest.h>
#include "streamr-dht/Identifiers.hpp"
#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include <gmock/gmock.h>
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::connection::PendingConnection;
using streamr::dht::connection::websocket::WebsocketClientConnector;
using streamr::dht::connection::websocket::WebsocketClientConnectorOptions;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::dht::transport::SendOptions;
using streamr::eventemitter::HandlerToken;

class DummyTransport : public streamr::dht::transport::Transport {
public:
  // void send(const Message& /*message*/, SendOptions /*opts*/) override {}
  //  void send(const streamr::dht::transport::transportevents::Message& /*message*/, SendOptions /*opts*/) override {}

    void send(
        const dht::Message& message, const SendOptions& sendOptions) override {}

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const override {
        return PeerDescriptor(); // Return a default-constructed PeerDescriptor
    }
    
    void stop() override {}

    

    
  
};

class WebsocketClientConnectorTest : public ::testing::Test {
protected:
  //  ListeningRpcCommunicator dummyCommunicator;
    std::unique_ptr<DummyTransport> dummyTransport;
   // ListeningRpcCommunicator dummyCommunicator;
    std::unique_ptr<ListeningRpcCommunicator> dummyCommunicator;
  
    std::unique_ptr<WebsocketClientConnector> connector;
    PeerDescriptor localPeerDescriptor;

    void SetUp() override {
        /*
        std::function<void(const streamr::dht::transport::transportevents::Message&)> dummyListener = [](const streamr::dht::transport::transportevents::Message& message) {
            
            std::cout << "Dummy listener received a message" << std::endl;
        };
*/
        dummyTransport = std::make_unique<DummyTransport>();
        dummyCommunicator = std::make_unique<ListeningRpcCommunicator>(
            streamr::dht::ServiceID("dummy_service"),
            *dummyTransport,
            std::nullopt
        );
/*
        dummyCommunicator{
            streamr::dht::ServiceID("dummy_service"),
            *dummyTransport,
            std::nullopt,
        };
*/

        WebsocketClientConnectorOptions options{
        .onNewConnection =
            [](const std::shared_ptr<PendingConnection>& /*connection*/) {
                return true;
            },
         .hasConnection = [](DhtAddress) { return false; },
         .rpcCommunicator = *dummyCommunicator    
        };
       
  

        // Create the connector
        connector = std::make_unique<WebsocketClientConnector>(std::move(options));

        // Set up local peer descriptor
        localPeerDescriptor = createMockPeerDescriptor("local", true);
        connector->setLocalPeerDescriptor(localPeerDescriptor);
  


           // Create a shared_ptr to ListeningRpcCommunicator

        // Set up options
        /*
        options.onNewConnection =
            [](const std::shared_ptr<PendingConnection>&) { return true; };
        options.hasConnection = [](DhtAddress) { return false; };
        options.rpcCommunicator = 

        // Create the connector
        connector = std::make_unique<WebsocketClientConnector>(std::move(options));

        // Set up local peer descriptor
        localPeerDescriptor = createMockPeerDescriptor("local", true);
        connector->setLocalPeerDescriptor(localPeerDescriptor);
   */
        /*
            // Create a shared_ptr to RpcCommunicatorType
        communicator = std::make_shared<RpcCommunicatorType>();

        // Set up options
        options.onNewConnection =
            [](const std::shared_ptr<PendingConnection>&) { return true; };
        options.hasConnection = [](DhtAddress) { return false; };

  // Create the connector, passing only options
        connector = std::make_unique<WebsocketClientConnector>(std::move(options));

        // Set up local peer descriptor
        localPeerDescriptor = createMockPeerDescriptor("local", true);
        connector->setLocalPeerDescriptor(localPeerDescriptor);

        // Set the communicator after construction if needed
        connector->setCommunicator(communicator);
 */
/*
        // Create the connector, passing both options and communicator
        connector = std::make_unique<WebsocketClientConnector>(std::move(options), communicator);

        // Set up local peer descriptor
        localPeerDescriptor = createMockPeerDescriptor("local", true);
        connector->setLocalPeerDescriptor(localPeerDescriptor);
*/
        /*
        // Set up options
        communicator = std::make_shared<RpcCommunicatorType>();
        options.onNewConnection =
            [](const std::shared_ptr<PendingConnection>&) { return true; };
        options.hasConnection = [](DhtAddress) { return false; };
        options.rpcCommunicator = communicator;

        connector =
            std::make_unique<WebsocketClientConnector>(std::move(options));

        // Set up local peer descriptor
        localPeerDescriptor = createMockPeerDescriptor("local", true);
        connector->setLocalPeerDescriptor(localPeerDescriptor);
        */
    }

    static PeerDescriptor createMockPeerDescriptor(
        const std::string& id, bool hasWebsocket) {
        PeerDescriptor pd;
        
        pd.set_nodeid(id);
        if (hasWebsocket) {
            auto* ws = pd.mutable_websocket();
            ws->set_host("localhost");
            ws->set_port(8080);
            ws->set_tls(false);
        }
        
        return pd;
    }
};


TEST_F(WebsocketClientConnectorTest, IsPossibleToFormConnectionLocalNodeWithoutWebsocketRemoteNodeWithWebsocket) {
    connector->setLocalPeerDescriptor(createMockPeerDescriptor("local", false));
    auto remotePd = createMockPeerDescriptor("remote", true);
    EXPECT_TRUE(connector->isPossibleToFormConnection(remotePd));
}

TEST_F(WebsocketClientConnectorTest, IsPossibleToFormConnectionLocalNodeWithWebsocketRemoteNodeWithoutWebsocket) {
    auto remotePd = createMockPeerDescriptor("remote", false);
    EXPECT_FALSE(connector->isPossibleToFormConnection(remotePd));
}

TEST_F(WebsocketClientConnectorTest, IsPossibleToFormConnectionBothNodesWithWebsocket) {
    auto remotePd = createMockPeerDescriptor("remote", true);
    EXPECT_FALSE(connector->isPossibleToFormConnection(remotePd));
}

TEST_F(WebsocketClientConnectorTest, IsPossibleToFormConnectionBothNodesWithoutWebsocket) {
    connector->setLocalPeerDescriptor(createMockPeerDescriptor("local", false));
    auto remotePd = createMockPeerDescriptor("remote", false);
    EXPECT_FALSE(connector->isPossibleToFormConnection(remotePd));
}

TEST_F(WebsocketClientConnectorTest, IsPossibleToFormConnectionSameNode) {
    EXPECT_FALSE(connector->isPossibleToFormConnection(localPeerDescriptor));
}

TEST_F(WebsocketClientConnectorTest, IsPossibleToFormConnectionNullNodeId) {
    auto remotePd = createMockPeerDescriptor("", true);
    EXPECT_FALSE(connector->isPossibleToFormConnection(remotePd));
}