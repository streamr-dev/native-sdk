#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "streamr-dht/connection/ConnectionManager.hpp"
#include "streamr-dht/connection/ConnectorFacade.hpp"
#include "streamr-dht/transport/FakeTransport.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-trackerless-network/logic/proxy/ProxyClient.hpp"
#include "streamr-utils/BinaryUtils.hpp"
#include "streamr-utils/EthereumAddress.hpp"
#include "streamr-utils/StreamPartID.hpp"

using ::dht::ConnectivityMethod;
using ::dht::ConnectivityResponse;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::transport::FakeEnvironment;
using streamr::logger::SLogger;
using streamr::trackerlessnetwork::proxy::ProxyClient;
using streamr::trackerlessnetwork::proxy::ProxyClientOptions;
using streamr::utils::BinaryUtils;
using streamr::utils::EthereumAddress;
using streamr::utils::StreamPartID;

PeerDescriptor createLocalPeerDescriptor() {
    PeerDescriptor peerDescriptor;
    peerDescriptor.set_nodeid("1234");
    peerDescriptor.set_type(NodeType::NODEJS);
    peerDescriptor.set_publickey("");
    peerDescriptor.set_signature("");
    peerDescriptor.set_region(1);
    peerDescriptor.set_ipaddress(0);
    return peerDescriptor;
}

std::shared_ptr<ConnectionManager> createConnectionManager(
    const DefaultConnectorFacadeOptions& opts) {
    SLogger::info("Calling connection manager constructor");

    ConnectionManagerOptions connectionManagerOptions{
        .createConnectorFacade =
            [&opts]() -> std::shared_ptr<DefaultConnectorFacade> {
            return std::make_shared<DefaultConnectorFacade>(opts);
        }};
    return std::make_shared<ConnectionManager>(
        std::move(connectionManagerOptions));
}

TEST(ProxyClientTsIntegrationTest, ItCanPublishAMessage) {
    PeerDescriptor proxyPeerDescriptor;

    proxyPeerDescriptor.set_nodeid(BinaryUtils::hexToBinaryString(
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    proxyPeerDescriptor.set_type(NodeType::NODEJS);
    ConnectivityMethod connectivityMethod;
    connectivityMethod.set_host("127.0.0.1");
    connectivityMethod.set_port(44211); // NOLINT
    connectivityMethod.set_tls(false);

    proxyPeerDescriptor.mutable_websocket()->CopyFrom(connectivityMethod);

    FakeEnvironment fakeEnvironment;
    auto fakeTransport =
        fakeEnvironment.createTransport(createLocalPeerDescriptor());

    auto localPeerDescriptor = createLocalPeerDescriptor();
    auto connectionManager =
        createConnectionManager(DefaultConnectorFacadeOptions{
            .transport = *fakeTransport,
            .createLocalPeerDescriptor =
                [&localPeerDescriptor](
                    const ConnectivityResponse& /* response */)
                -> PeerDescriptor { return localPeerDescriptor; }});
    SLogger::info("Starting connection manager");
    connectionManager->start();

    ProxyClient proxyClient(ProxyClientOptions{
        .transport = *connectionManager,
        .localPeerDescriptor = createLocalPeerDescriptor(),
        .streamPartId =
            StreamPartID{"0xa000000000000000000000000000000000000000#01"},
        .connectionLocker = *connectionManager});

    proxyClient.start();

    proxyClient.setProxies(
        {proxyPeerDescriptor},
        ProxyDirection::PUBLISH,
        EthereumAddress("0xABB0a4a3981c854615C9bBDd9a358488Ff2b8762"));

    StreamMessage message;
    message.mutable_contentmessage()->set_content("Hello, world!");
    MessageID messageId;
    messageId.set_sequencenumber(1);
    message.mutable_messageid()->CopyFrom(messageId);

    proxyClient.broadcast(message);
}