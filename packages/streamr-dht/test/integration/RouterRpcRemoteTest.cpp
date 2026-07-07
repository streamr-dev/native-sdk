// Ported from packages/dht/test/integration/RouterRpcRemote.test.ts
// (v103.8.0-rc.3): two RpcCommunicators wired together, the server side
// answering routeMessage, exercising RouterRpcRemote's happy and error
// paths.
#include <optional>
#include <stdexcept>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.protos;
import streamr.dht.DhtRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.RouterRpcRemote;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageWrapper;
using ::protorpc::RpcMessage;
using streamr::dht::routing::RouterRpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::protorpc::RpcCommunicator;
using streamr::utils::blockingWait;

using RouterRpcClient = ::dht::RouterRpcClient<DhtCallContext>;
using RpcCommunicatorType = RpcCommunicator<DhtCallContext>;

namespace {

constexpr auto serviceId = "test";

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
RouteMessageWrapper createRoutedMessage(
    const PeerDescriptor& source, const PeerDescriptor& destination) {
    Message message;
    message.set_serviceid(serviceId);
    message.set_messageid("routed");
    RouteMessageWrapper routed;
    routed.set_requestid("routed");
    *routed.mutable_message() = message;
    *routed.mutable_sourcepeer() = source;
    routed.set_target(destination.nodeid());
    return routed;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

} // namespace

class RouterRpcRemoteTest : public ::testing::Test {
protected:
    RpcCommunicatorType clientCommunicator;
    RpcCommunicatorType serverCommunicator;
    PeerDescriptor clientPeerDescriptor = createMockPeerDescriptor();
    PeerDescriptor serverPeerDescriptor = createMockPeerDescriptor();
    bool routeMessageShouldThrow = false;
    std::optional<RouterRpcRemote> rpcRemote;

    void SetUp() override {
        serverCommunicator
            .registerRpcMethod<RouteMessageWrapper, RouteMessageAck>(
                "routeMessage",
                [this](
                    const RouteMessageWrapper& request,
                    const DhtCallContext& /*context*/) -> RouteMessageAck {
                    if (this->routeMessageShouldThrow) {
                        throw std::runtime_error("Route message error");
                    }
                    RouteMessageAck ack;
                    ack.set_requestid(request.requestid());
                    return ack;
                });
        this->clientCommunicator.setOutgoingMessageCallback(
            [this](
                const RpcMessage& message,
                const std::string& /*requestId*/,
                const DhtCallContext& /*context*/) {
                this->serverCommunicator.handleIncomingMessage(
                    message, DhtCallContext());
            });
        this->serverCommunicator.setOutgoingMessageCallback(
            [this](
                const RpcMessage& message,
                const std::string& /*requestId*/,
                const DhtCallContext& /*context*/) {
                this->clientCommunicator.handleIncomingMessage(
                    message, DhtCallContext());
            });
        this->rpcRemote.emplace(
            clientPeerDescriptor,
            serverPeerDescriptor,
            RouterRpcClient(this->clientCommunicator));
    }
};

TEST_F(RouterRpcRemoteTest, RouteMessageHappyPath) {
    const bool routable = blockingWait(this->rpcRemote->routeMessage(
        createRoutedMessage(clientPeerDescriptor, serverPeerDescriptor)));
    EXPECT_TRUE(routable);
}

TEST_F(RouterRpcRemoteTest, RouteMessageErrorPath) {
    this->routeMessageShouldThrow = true;
    const bool routable = blockingWait(this->rpcRemote->routeMessage(
        createRoutedMessage(clientPeerDescriptor, serverPeerDescriptor)));
    EXPECT_FALSE(routable);
}
