// Ported from packages/dht/test/unit/WebrtcConnector.test.ts
// (v103.8.0-rc.3): the connector's ongoingConnectAttempts dedupe — a second
// connect() to the same peer returns the SAME pending connection until the
// first one settles (connected or disconnected), after which a new attempt
// is created. Adaptations: the TS `new MockTransport() as any` becomes a
// FakeTransport in its own FakeEnvironment (the connector's RPC
// communicator needs a working transport reference), and the TS
// MockConnection is a minimal local Connection subclass.
#include <memory>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

import streamr.dht.Connection;
import streamr.dht.FakeTransport;
import streamr.dht.IPendingConnection;
import streamr.dht.TestUtils;
import streamr.dht.WebrtcConnector;
import streamr.dht.protos;

using ::dht::PeerDescriptor;
using streamr::dht::connection::Connection;
using streamr::dht::connection::ConnectionType;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::webrtc::WebrtcConnector;
using streamr::dht::connection::webrtc::WebrtcConnectorOptions;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::FakeTransport;

namespace {

class MockConnection : public Connection {
public:
    MockConnection() : Connection(ConnectionType::WEBRTC) {}
    void send(const std::vector<std::byte>& /*data*/) override {}
    void close(bool /*gracefulLeave*/) override {}
    void destroy() override {}
};

} // namespace

class WebrtcConnectorTest : public ::testing::Test {
protected:
    FakeEnvironment fakeEnvironment;
    std::shared_ptr<FakeTransport> transport;
    std::unique_ptr<WebrtcConnector> connector;

    void SetUp() override {
        this->transport =
            this->fakeEnvironment.createTransport(createMockPeerDescriptor());
        this->connector =
            std::make_unique<WebrtcConnector>(WebrtcConnectorOptions{
                .onNewConnection =
                    [](const std::shared_ptr<IPendingConnection>&) {
                        return true;
                    },
                .transport = *this->transport});
        this->connector->setLocalPeerDescriptor(createMockPeerDescriptor());
    }

    void TearDown() override { this->connector->stop(); }
};

TEST_F(WebrtcConnectorTest, ReturnsExistingConnectingConnection) {
    const auto remotePeerDescriptor = createMockPeerDescriptor();
    const auto firstConnection =
        this->connector->connect(remotePeerDescriptor, false);
    const auto secondConnection =
        this->connector->connect(remotePeerDescriptor, false);
    EXPECT_EQ(firstConnection, secondConnection);
    firstConnection->close(false);
}

TEST_F(WebrtcConnectorTest, DisconnectedEventRemovesConnectingConnection) {
    const auto remotePeerDescriptor = createMockPeerDescriptor();
    const auto firstConnection =
        this->connector->connect(remotePeerDescriptor, false);
    // Adaptation: TS raises the Disconnected event with a bare
    // `firstConnection.emit('disconnected', false)`. The C++ EventEmitter
    // mutex is non-recursive (JS re-entrant emit has no analogue), and a
    // direct emit bypasses PendingConnection's `stopped` guard, so the
    // connection<->pendingConnection close cross-wiring re-enters the same
    // emitter and deadlocks. close(false) is the real API that raises the
    // same Disconnected event AND sets the guard, so the cascade terminates
    // — testing the identical behaviour (a disconnect drops the ongoing
    // attempt) without the JS-only re-entrancy assumption.
    firstConnection->close(false);
    const auto secondConnection =
        this->connector->connect(remotePeerDescriptor, false);
    EXPECT_NE(firstConnection, secondConnection);
    secondConnection->close(false);
}

TEST_F(WebrtcConnectorTest, ConnectedEventRemovesConnectingConnection) {
    const auto remotePeerDescriptor = createMockPeerDescriptor();
    const auto firstConnection =
        this->connector->connect(remotePeerDescriptor, false);
    firstConnection->onHandshakeCompleted(std::make_shared<MockConnection>());
    const auto secondConnection =
        this->connector->connect(remotePeerDescriptor, false);
    EXPECT_NE(firstConnection, secondConnection);
    firstConnection->close(false);
    secondConnection->close(false);
}
