// Ported from packages/dht/test/unit/RecursiveOperationManager.test.ts
// (v103.8.0-rc.3): exercises the routeRequest server path (success,
// no-targets, duplicate, bad-payload) and the no-connections execute path.
// The Router is substituted by callbacks (the C++ Router is concrete);
// MockTransport / MockConnectionsView stand in for the transport.
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.utils.Uuid;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.protos;
import streamr.dht.ConnectionsView;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.Identifiers;
import streamr.dht.LocalDataStore;
import streamr.dht.RecursiveOperationManager;
import streamr.dht.RoutingSession;
import streamr.dht.Transport;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperation;
using ::dht::RecursiveOperationRequest;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageError;
using ::dht::RouteMessageWrapper;
using ::protorpc::RpcMessage;
using streamr::dht::DhtAddress;
using streamr::dht::DhtNodeRpcRemote;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::recursiveoperation::RecursiveOperationManager;
using streamr::dht::recursiveoperation::RecursiveOperationManagerOptions;
using streamr::dht::routing::RoutingMode;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::store::LocalDataStore;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::testutils::createWrappedClosestPeersRequest;
using streamr::dht::transport::SendOptions;
using streamr::dht::transport::Transport;
using streamr::protorpc::RpcCommunicator;
using streamr::utils::blockingWait;
using streamr::utils::Uuid;

namespace {

constexpr uint32_t maxTtl = 3000;

// A real RpcCommunicator with a helper to invoke a registered handler by
// name; surfaces a handler exception as a thrown error (the response
// carries errorType).
//
// The server now responds ASYNCHRONOUSLY (handleIncomingMessage schedules
// the response on a worker and returns immediately), so the ack arrives on
// another thread. The ack response carries the same requestid as the
// request, so callRpcMethod waits for the matching response under a
// condition variable.
class FakeRpcCommunicator : public RpcCommunicator<DhtCallContext> {
private:
    static constexpr std::chrono::seconds ackWaitTimeout{5};

    std::mutex mutex;
    std::condition_variable cv;
    std::string captureRequestId;
    std::optional<RpcMessage> captured;

public:
    FakeRpcCommunicator() {
        this->setOutgoingMessageCallback(
            [this](
                const RpcMessage& message,
                const std::string& /*requestId*/,
                const DhtCallContext& /*context*/) {
                std::lock_guard lock(this->mutex);
                if (message.requestid() == this->captureRequestId) {
                    this->captured = message;
                    this->cv.notify_all();
                }
            });
    }

    template <typename Req, typename Resp>
    Resp callRpcMethod(const std::string& methodName, const Req& request) {
        RpcMessage requestMessage;
        requestMessage.mutable_body()->PackFrom(request);
        (*requestMessage.mutable_header())["method"] = methodName;
        (*requestMessage.mutable_header())["request"] = "request";
        requestMessage.set_requestid(Uuid::v4());
        {
            std::lock_guard lock(this->mutex);
            this->captured.reset();
            this->captureRequestId = requestMessage.requestid();
        }
        this->handleIncomingMessage(requestMessage, DhtCallContext());

        std::unique_lock lock(this->mutex);
        this->cv.wait_for(lock, ackWaitTimeout, [this]() {
            return this->captured.has_value();
        });
        Resp response;
        if (this->captured.has_value()) {
            if (this->captured->has_errortype()) {
                throw std::runtime_error("RPC handler returned an error");
            }
            this->captured->body().UnpackTo(&response);
        }
        return response;
    }
};

class MockTransport : public Transport {
public:
    size_t sendCount = 0;
    PeerDescriptor localPeerDescriptor = createMockPeerDescriptor();

    void send(const Message& /*msg*/, const SendOptions& /*opts*/) override {
        ++this->sendCount;
    }
    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const override {
        return this->localPeerDescriptor;
    }
    void stop() override {}
};

class MockConnectionsView : public ConnectionsView {
public:
    [[nodiscard]] std::vector<PeerDescriptor> getConnections() override {
        return {};
    }
    [[nodiscard]] size_t getConnectionCount() override { return 0; }
    [[nodiscard]] bool hasConnection(const DhtAddress& /*id*/) override {
        return false;
    }
};

} // namespace

class RecursiveOperationManagerTest : public ::testing::Test {
protected:
    FakeRpcCommunicator rpcCommunicator;
    MockTransport transport;
    MockConnectionsView connectionsView;
    LocalDataStore localDataStore{maxTtl};
    PeerDescriptor peerDescriptor1 = createMockPeerDescriptor();
    PeerDescriptor peerDescriptor2 = createMockPeerDescriptor();

    using RouteFn = std::function<RouteMessageAck(
        const RouteMessageWrapper&, RoutingMode, std::optional<DhtAddress>)>;

    [[nodiscard]] Message createMessage() const {
        RecursiveOperationRequest request;
        request.set_operation(RecursiveOperation::FIND_CLOSEST_NODES);
        request.set_sessionid(Uuid::v4());
        Message message;
        message.set_serviceid("unknown");
        message.set_messageid(Uuid::v4());
        *message.mutable_recursiveoperationrequest() = request;
        *message.mutable_sourcedescriptor() = this->peerDescriptor1;
        *message.mutable_targetdescriptor() = this->peerDescriptor2;
        return message;
    }

    [[nodiscard]] RouteMessageWrapper createRoutedMessage() const {
        RouteMessageWrapper routed;
        *routed.mutable_message() = this->createMessage();
        routed.set_requestid("REQ");
        *routed.mutable_sourcepeer() = this->peerDescriptor1;
        routed.set_target(this->peerDescriptor2.nodeid());
        return routed;
    }

    [[nodiscard]] std::shared_ptr<RecursiveOperationManager> makeManager(
        RouteFn doRouteMessage) {
        return RecursiveOperationManager::newInstance(
            RecursiveOperationManagerOptions{
                .rpcCommunicator = this->rpcCommunicator,
                .sessionTransport = this->transport,
                .connectionsView = this->connectionsView,
                .localPeerDescriptor = this->peerDescriptor1,
                .serviceId = ServiceID{"RecursiveOperationManager"},
                .localDataStore = this->localDataStore,
                .addContact = [](const PeerDescriptor& /*contact*/) {},
                .createDhtNodeRpcRemote =
                    [](const PeerDescriptor& /*peerDescriptor*/)
                    -> std::shared_ptr<DhtNodeRpcRemote> { return nullptr; },
                .doRouteMessage = std::move(doRouteMessage),
                .isMostLikelyDuplicate =
                    [](const std::string& /*requestId*/) { return false; },
                .addToDuplicateDetector =
                    [](const std::string& /*requestId*/) {}});
    }

    // The default mock router: an empty ack (no error).
    [[nodiscard]] std::shared_ptr<RecursiveOperationManager> makeManager() {
        return this->makeManager(
            [](const RouteMessageWrapper& /*message*/,
               RoutingMode /*mode*/,
               const std::optional<DhtAddress>& /*excludedPeer*/) {
                return RouteMessageAck{};
            });
    }

    // A mock router whose doRouteMessage returns the given error.
    [[nodiscard]] static RouteFn routerReturning(RouteMessageError error) {
        return [error](
                   const RouteMessageWrapper& message,
                   RoutingMode /*mode*/,
                   const std::optional<DhtAddress>& /*excludedPeer*/) {
            RouteMessageAck ack;
            ack.set_requestid(message.requestid());
            ack.set_error(error);
            return ack;
        };
    }
};

TEST_F(RecursiveOperationManagerTest, Server) {
    auto manager = this->makeManager();
    const auto ack = this->rpcCommunicator
                         .callRpcMethod<RouteMessageWrapper, RouteMessageAck>(
                             "routeRequest", this->createRoutedMessage());
    EXPECT_FALSE(ack.has_error());
    manager->stop();
}

TEST_F(RecursiveOperationManagerTest, FindClosestNodesReturnsSelfIfNoPeers) {
    auto manager = this->makeManager();
    const auto result = blockingWait(manager->execute(
        Identifiers::createRandomDhtAddress(),
        RecursiveOperation::FIND_CLOSEST_NODES));
    ASSERT_FALSE(result.closestNodes.empty());
    EXPECT_TRUE(
        Identifiers::areEqualPeerDescriptors(
            result.closestNodes[0], this->peerDescriptor1));
    manager->stop();
}

TEST_F(RecursiveOperationManagerTest, ServerThrowsIfPayloadNotRecursive) {
    auto manager = this->makeManager();
    Message badMessage;
    badMessage.set_serviceid("unknown");
    badMessage.set_messageid(Uuid::v4());
    *badMessage.mutable_rpcmessage() =
        createWrappedClosestPeersRequest(this->peerDescriptor1);
    *badMessage.mutable_sourcedescriptor() = this->peerDescriptor2;
    *badMessage.mutable_targetdescriptor() = this->peerDescriptor2;
    RouteMessageWrapper routed;
    *routed.mutable_message() = badMessage;
    routed.set_requestid("REQ");
    *routed.mutable_sourcepeer() = this->peerDescriptor2;
    routed.set_target(this->peerDescriptor1.nodeid());
    EXPECT_THROW(
        (this->rpcCommunicator
             .callRpcMethod<RouteMessageWrapper, RouteMessageAck>(
                 "routeRequest", routed)),
        std::exception);
    manager->stop();
}

TEST_F(RecursiveOperationManagerTest, NoTargets) {
    auto manager =
        this->makeManager(routerReturning(RouteMessageError::NO_TARGETS));
    const auto ack = this->rpcCommunicator
                         .callRpcMethod<RouteMessageWrapper, RouteMessageAck>(
                             "routeRequest", this->createRoutedMessage());
    ASSERT_TRUE(ack.has_error());
    EXPECT_EQ(ack.error(), RouteMessageError::NO_TARGETS);
    // The response send is a detached task; stop() drains it, so the
    // sendCount observation after stop() is deterministic.
    manager->stop();
    EXPECT_EQ(this->transport.sendCount, 1U);
}

TEST_F(RecursiveOperationManagerTest, Error) {
    auto manager =
        this->makeManager(routerReturning(RouteMessageError::DUPLICATE));
    const auto ack = this->rpcCommunicator
                         .callRpcMethod<RouteMessageWrapper, RouteMessageAck>(
                             "routeRequest", this->createRoutedMessage());
    ASSERT_TRUE(ack.has_error());
    EXPECT_EQ(ack.error(), RouteMessageError::DUPLICATE);
    // stop() drains the detached response-send scope first, so a zero
    // sendCount after it proves no response was (or will be) sent.
    manager->stop();
    EXPECT_EQ(this->transport.sendCount, 0U);
}
