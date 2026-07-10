// Ported from packages/dht/test/integration/ConnectivityChecking.test.ts
// (v103.8.0-rc.3): a ConnectionManager with a real websocket server answers
// a connectivity request sent with the client-side connectivityChecker.
// Adaptation: TS backs the connector facade with a no-op MockTransport; here
// a FakeTransport in its own FakeEnvironment serves the same purpose (the
// connector RPC communicator is not exercised by this test).
#include <memory>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

import streamr.dht.ConnectionManager;
import streamr.dht.ConnectorFacade;
import streamr.dht.FakeTransport;
import streamr.dht.PortRange;
import streamr.dht.TestUtils;
import streamr.dht.Version;
import streamr.dht.connectivityChecker;
import streamr.dht.protos;

using ::dht::ConnectivityRequest;
using ::dht::ConnectivityResponse;
using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::connection::sendConnectivityRequest;
using streamr::dht::helpers::Version;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::FakeTransport;
using streamr::dht::types::PortRange;

namespace {

constexpr uint16_t serverPort = 15000;
constexpr auto serverHost = "127.0.0.1";

} // namespace

class ConnectivityCheckingTest : public ::testing::Test {
protected:
    FakeEnvironment fakeEnvironment;
    std::shared_ptr<FakeTransport> transport;
    std::shared_ptr<ConnectionManager> server;

    void SetUp() override {
        this->transport =
            this->fakeEnvironment.createTransport(createMockPeerDescriptor());
        DefaultConnectorFacadeOptions facadeOptions{
            .transport = *this->transport,
            .websocketHost = serverHost,
            .websocketPortRange =
                PortRange{.min = serverPort, .max = serverPort},
            .createLocalPeerDescriptor =
                [](const ConnectivityResponse& /* response */)
                -> PeerDescriptor {
                auto descriptor = createMockPeerDescriptor();
                descriptor.mutable_websocket()->set_host(serverHost);
                descriptor.mutable_websocket()->set_port(serverPort);
                descriptor.mutable_websocket()->set_tls(false);
                return descriptor;
            }};
        this->server =
            std::make_shared<ConnectionManager>(ConnectionManagerOptions{
                .createConnectorFacade = [opts = std::move(facadeOptions)]()
                    -> std::shared_ptr<DefaultConnectorFacade> {
                    return std::make_shared<DefaultConnectorFacade>(opts);
                }});
        this->server->start();
    }

    void TearDown() override { this->server->stop(); }
};

TEST_F(ConnectivityCheckingTest, ConnectivityCheckWithCompatibleVersion) {
    ConnectivityRequest request;
    request.set_host(serverHost);
    request.set_port(serverPort);
    request.set_tls(false);
    request.set_allowselfsignedcertificate(false);

    const auto response = sendConnectivityRequest(
        request, this->server->getLocalPeerDescriptor());
    EXPECT_EQ(response.protocolversion(), Version::localProtocolVersion);
}
