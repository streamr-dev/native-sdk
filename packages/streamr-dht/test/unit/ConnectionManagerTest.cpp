#include "streamr-dht/connection/ConnectionManager.hpp"
#include <memory>
#include <string>
#include <gtest/gtest.h>
#include <rtc/rtc.hpp>
#include <folly/experimental/coro/BlockingWait.h>
#include <folly/experimental/coro/Collect.h>
#include <folly/experimental/coro/Promise.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"
#include "streamr-dht/helpers/Errors.hpp"
#include "streamr-dht/transport/FakeTransport.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-dht/types/PortRange.hpp"
#include "streamr-logger/SLogger.hpp"

using ::dht::ConnectivityResponse;
using ::dht::Message;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
using ::protorpc::RpcMessage;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::helpers::SendFailed;
using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::FakeTransport;
using streamr::dht::transport::SendOptions;
using streamr::dht::types::PortRange;
using streamr::logger::SLogger;

namespace transportevents = streamr::dht::transport::transportevents;

PeerDescriptor createMockPeerDescriptor(uint16_t websocketPort) {
    PeerDescriptor peerDescriptor;
    peerDescriptor.set_nodeid(std::to_string(websocketPort)/*""+Identifiers::getRawFromDhtAddress(
        Identifiers::createRandomDhtAddress())*/ );
    peerDescriptor.set_type(NodeType::NODEJS);
    peerDescriptor.mutable_websocket()->set_port(websocketPort);
    peerDescriptor.mutable_websocket()->set_host(std::string("127.0.0.1"));
    peerDescriptor.mutable_websocket()->set_tls(false);
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

constexpr auto SERVICE_ID = "demo"; // NOLINT
constexpr uint16_t mockWebsocketPort1 = 9995;
constexpr uint16_t mockWebsocketPort2 = 9996;

class ConnectionManagerTest : public ::testing::Test {
protected:
    // NOLINTBEGIN
    PeerDescriptor mockPeerDescriptor1 =
        createMockPeerDescriptor(mockWebsocketPort1);
    PeerDescriptor mockPeerDescriptor2 =
        createMockPeerDescriptor(mockWebsocketPort2);

    FakeEnvironment fakeEnvironment;
    std::shared_ptr<FakeTransport> mockConnectorTransport1;
    std::shared_ptr<FakeTransport> mockConnectorTransport2;

    // NOLINTEND

    void SetUp() override {}

    void TearDown() override {}

public:
    ConnectionManagerTest()
        : mockConnectorTransport1(
              fakeEnvironment.createTransport(mockPeerDescriptor1)),
          mockConnectorTransport2(
              fakeEnvironment.createTransport(mockPeerDescriptor2)) {}
};

TEST_F(
    ConnectionManagerTest, CanSendDataToOtherConnectionmanagerOverWebsocket) {
    rtc::InitLogger(rtc::LogLevel::Verbose);
    SLogger::trace("In the beginning");
    auto connectionManager1 =
        createConnectionManager(DefaultConnectorFacadeOptions{
            .transport = *mockConnectorTransport1,
            .websocketHost = "127.0.0.1",
            .websocketPortRange =
                PortRange{
                    .min = mockWebsocketPort1,
                    .max = mockWebsocketPort1}, // NOLINT
            .createLocalPeerDescriptor =
                [this](const ConnectivityResponse& /* response */)
                -> PeerDescriptor { return mockPeerDescriptor1; }});
    SLogger::info("Starting connection manager 1");
    connectionManager1->start();

    auto connectionManager2 =
        createConnectionManager(DefaultConnectorFacadeOptions{
            .transport = *mockConnectorTransport2,
            .websocketHost = "127.0.0.1",
            .websocketPortRange =
                PortRange{
                    .min = mockWebsocketPort2,
                    .max = mockWebsocketPort2}, // NOLINT
            .createLocalPeerDescriptor =
                [this](const ConnectivityResponse& /* response */)
                -> PeerDescriptor { return mockPeerDescriptor2; }});

    SLogger::info("Starting connection manager 2");
    connectionManager2->start();

    auto promiseContract = folly::coro::makePromiseContract<void>();

    auto connectedPromiseContract1 = folly::coro::makePromiseContract<void>();
    auto connectedPromiseContract2 = folly::coro::makePromiseContract<void>();

    connectionManager2->on<transportevents::Message>(
        [&promiseContract](const Message& /* message*/) mutable {
            // EXPECT_EQ(message.body().oneof_kind(), Message::RpcMessage);
            promiseContract.first.setValue();
        });

    connectionManager1->on<transportevents::Connected>(
        [&connectedPromiseContract1](
            const PeerDescriptor& /* peerDescriptor*/) mutable {
            connectedPromiseContract1.first.setValue();
        });

    connectionManager2->on<transportevents::Connected>(
        [&connectedPromiseContract2](
            const PeerDescriptor& /* peerDescriptor*/) mutable {
            connectedPromiseContract2.first.setValue();
        });

    Message msg;
    RpcMessage rpcMessage;
    msg.set_serviceid(SERVICE_ID);
    msg.set_messageid("1");
    msg.mutable_rpcmessage()->CopyFrom(rpcMessage);

    msg.mutable_targetdescriptor()->CopyFrom(
        connectionManager2->getLocalPeerDescriptor());

    SLogger::info(
        "Sending message from connection manager 1 to connection manager 2 ");

    connectionManager1->send(msg, SendOptions{.connect = true});

    folly::coro::blockingWait(folly::coro::collectAll(
        std::move(promiseContract.second),
        std::move(connectedPromiseContract1.second),
        std::move(connectedPromiseContract2.second)));

    connectionManager1->stop();
    connectionManager2->stop();
}

TEST_F(
    ConnectionManagerTest, ReportsCorrectErrorIfConnectingToNonExistentPort) {
    auto connectionManager1 =
        createConnectionManager(DefaultConnectorFacadeOptions{
            .transport = *mockConnectorTransport1,
            .websocketHost = "127.0.0.1",
            .websocketPortRange =
                PortRange{
                    .min = mockWebsocketPort1,
                    .max = mockWebsocketPort1}, // NOLINT
            .createLocalPeerDescriptor =
                [this](const ConnectivityResponse& /* response */)
                -> PeerDescriptor { return mockPeerDescriptor1; }});
    SLogger::info("Starting connection manager 1");
    connectionManager1->start();

    Message msg;
    msg.set_serviceid(SERVICE_ID);
    msg.set_messageid("1");
    msg.mutable_rpcmessage()->CopyFrom(RpcMessage());
    msg.mutable_targetdescriptor()->CopyFrom(createMockPeerDescriptor(0));

    EXPECT_THROW(
        connectionManager1->send(msg, SendOptions{.connect = true}),
        SendFailed);

    SLogger::info("Stopping connection manager 1");
    connectionManager1->stop();
    SLogger::info("Connection manager 1 stopped");
}
