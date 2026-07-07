// Ported from packages/dht/test/unit/Router.test.ts (v103.8.0-rc.3).
//
// Router registers its routeMessage/forwardMessage handlers on an
// RpcCommunicator; the test drives them through a FakeRpcCommunicator that
// invokes the registered handler by name (the TS test's FakeRpcCommunicator
// captures the handler and calls it directly; here it goes through the real
// server registry via handleIncomingMessage and unpacks the response ack).
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <gtest/gtest.h>

import streamr.utils.Uuid;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;
import streamr.dht.Router;
import streamr.dht.RouterRpcLocal;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageError;
using ::dht::RouteMessageWrapper;
using ::protorpc::RpcMessage;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::routing::Router;
using streamr::dht::routing::RouterOptions;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::protorpc::RpcCommunicator;
using streamr::utils::Uuid;

namespace {

// A real RpcCommunicator (so Router can register on it and build remotes
// from it) with a helper to invoke a registered handler by name and hand
// back the unpacked response, standing in for the TS FakeRpcCommunicator.
//
// routeMessage/forwardMessage are now ASYNC handlers: handleIncomingMessage
// schedules the response on a worker and returns immediately, so the ack
// arrives on another thread. The ack response carries the same requestid as
// the request, which distinguishes it from the outgoing forward requests the
// Router itself sends (those have fresh requestids). callRpcMethod waits for
// the matching ack under a condition variable.
class FakeRpcCommunicator : public RpcCommunicator<DhtCallContext> {
private:
    // Upper bound for the async ack to come back; routing acks return in
    // milliseconds, so this only bounds a hang.
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
            this->captured->body().UnpackTo(&response);
        }
        return response;
    }
};

} // namespace

class RouterTest : public ::testing::Test {
protected:
    FakeRpcCommunicator rpcCommunicator;
    PeerDescriptor peerDescriptor1 = createMockPeerDescriptor();
    PeerDescriptor peerDescriptor2 = createMockPeerDescriptor();
    std::map<DhtAddress, PeerDescriptor> connections;
    std::shared_ptr<Router> router;

    [[nodiscard]] Message createMessage() const {
        Message message;
        message.set_serviceid("unknown");
        message.set_messageid(Uuid::v4());
        *message.mutable_sourcedescriptor() = this->peerDescriptor1;
        *message.mutable_targetdescriptor() = this->peerDescriptor2;
        return message;
    }

    // The message whose target is a third node (peerDescriptor2), i.e. this
    // node must route it onward.
    [[nodiscard]] RouteMessageWrapper createForwardTarget() const {
        RouteMessageWrapper routed;
        *routed.mutable_message() = this->createMessage();
        routed.set_requestid(Uuid::v4());
        routed.set_target(this->peerDescriptor2.nodeid());
        *routed.mutable_sourcepeer() = this->peerDescriptor1;
        return routed;
    }

    // The message whose target is this node (peerDescriptor1).
    [[nodiscard]] RouteMessageWrapper createSelfTarget() const {
        RouteMessageWrapper routed;
        *routed.mutable_message() = this->createMessage();
        routed.set_requestid("REQ");
        routed.set_target(this->peerDescriptor1.nodeid());
        *routed.mutable_sourcepeer() = this->peerDescriptor2;
        return routed;
    }

    void SetUp() override {
        this->router = Router::newInstance(
            RouterOptions{
                .rpcCommunicator = this->rpcCommunicator,
                .localPeerDescriptor = this->peerDescriptor1,
                .handleMessage = [](const Message& /*message*/) {},
                .getConnections =
                    [this]() {
                        std::vector<PeerDescriptor> result;
                        result.reserve(this->connections.size());
                        for (const auto& [id, descriptor] : this->connections) {
                            result.push_back(descriptor);
                        }
                        return result;
                    }});
    }

    void TearDown() override {
        if (this->router) {
            this->router->stop();
        }
    }

    void addConnection(const PeerDescriptor& descriptor) {
        this->connections[Identifiers::getNodeIdFromPeerDescriptor(
            descriptor)] = descriptor;
    }

    RouteMessageAck route(const RouteMessageWrapper& message) {
        return this->rpcCommunicator
            .callRpcMethod<RouteMessageWrapper, RouteMessageAck>(
                "routeMessage", message);
    }

    RouteMessageAck forward(const RouteMessageWrapper& message) {
        return this->rpcCommunicator
            .callRpcMethod<RouteMessageWrapper, RouteMessageAck>(
                "forwardMessage", message);
    }
};

TEST_F(RouterTest, DoRouteMessageWithoutConnections) {
    const auto ack = route(createForwardTarget());
    ASSERT_TRUE(ack.has_error());
    EXPECT_EQ(ack.error(), RouteMessageError::NO_TARGETS);
}

TEST_F(RouterTest, DoRouteMessageWithConnections) {
    addConnection(peerDescriptor2);
    const auto ack = route(createForwardTarget());
    EXPECT_FALSE(ack.has_error());
}

TEST_F(RouterTest, DoRouteMessageWithParallelRootNodeIds) {
    addConnection(peerDescriptor2);
    auto message = createForwardTarget();
    message.add_parallelrootnodeids(
        std::string(Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor2)));
    const auto ack = route(message);
    ASSERT_TRUE(ack.has_error());
    EXPECT_EQ(ack.error(), RouteMessageError::NO_TARGETS);
}

TEST_F(RouterTest, RouteServerIsDestinationWithoutConnections) {
    const auto ack = route(createSelfTarget());
    EXPECT_FALSE(ack.has_error());
}

TEST_F(RouterTest, RouteServerWithConnections) {
    addConnection(peerDescriptor2);
    const auto ack = route(createSelfTarget());
    EXPECT_FALSE(ack.has_error());
}

TEST_F(RouterTest, RouteServerOnDuplicateMessage) {
    const auto message = createSelfTarget();
    router->addToDuplicateDetector(message.requestid());
    const auto ack = route(message);
    ASSERT_TRUE(ack.has_error());
    EXPECT_EQ(ack.error(), RouteMessageError::DUPLICATE);
}

TEST_F(RouterTest, ForwardServerNoConnections) {
    const auto ack = forward(createSelfTarget());
    ASSERT_TRUE(ack.has_error());
    EXPECT_EQ(ack.error(), RouteMessageError::NO_TARGETS);
}

TEST_F(RouterTest, ForwardServerWithConnections) {
    addConnection(peerDescriptor2);
    const auto ack = forward(createSelfTarget());
    EXPECT_FALSE(ack.has_error());
}

TEST_F(RouterTest, ForwardServerOnDuplicateMessage) {
    const auto message = createSelfTarget();
    router->addToDuplicateDetector(message.requestid());
    const auto ack = forward(message);
    ASSERT_TRUE(ack.has_error());
    EXPECT_EQ(ack.error(), RouteMessageError::DUPLICATE);
}
