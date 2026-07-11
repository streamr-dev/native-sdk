// Ported from packages/dht/test/unit/RecursiveOperationSession.test.ts
// (v103.8.0-rc.3): the initiator session collects sendResponse reports over
// a FakeEnvironment network and completes (here via the fallback timeout),
// exposing the union of the reported closest nodes.
#include <cstddef>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;
import streamr.dht.DhtRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.FakeTransport;
import streamr.dht.Identifiers;
import streamr.dht.RecursiveOperationSession;
import streamr.dht.RecursiveOperationSessionRpcRemote;
import streamr.dht.RoutingRpcCommunicator;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperation;
using ::dht::RouteMessageAck;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::recursiveoperation::RecursiveOperationSession;
using streamr::dht::recursiveoperation::RecursiveOperationSessionOptions;
using streamr::dht::recursiveoperation::RecursiveOperationSessionRpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::RoutingRpcCommunicator;
using streamr::dht::transport::SendOptions;
using streamr::utils::blockingWait;
using streamr::utils::waitForCondition;

using RecursiveOperationSessionRpcClient =
    ::dht::RecursiveOperationSessionRpcClient<DhtCallContext>;

namespace {
constexpr size_t waitedCompletions = 3;
constexpr size_t responseCount = 3;
constexpr size_t nodesPerResponse = 2;
} // namespace

class RecursiveOperationSessionTest : public ::testing::Test {
protected:
    FakeEnvironment environment;
    PeerDescriptor localPeerDescriptor = createMockPeerDescriptor();
    bool doRouteRequestCalled = false;
    std::shared_ptr<RecursiveOperationSession> session;

    void sendResponseFrom(const ServiceID& serviceId) {
        const auto mockPeerDescriptor = createMockPeerDescriptor();
        auto transport = this->environment.createTransport(mockPeerDescriptor);
        RoutingRpcCommunicator communicator(
            ServiceID{serviceId},
            [transport](const Message& message, const SendOptions& opts) {
                transport->send(message, opts);
            });
        RecursiveOperationSessionRpcRemote remote(
            mockPeerDescriptor,
            this->localPeerDescriptor,
            RecursiveOperationSessionRpcClient(communicator));
        // sendResponse is now a coroutine (fire-and-forget notify);
        // blocking here is fine, the test thread is not a pool worker.
        blockingWait(remote.sendResponse(
            {createMockPeerDescriptor(), createMockPeerDescriptor()},
            {createMockPeerDescriptor(), createMockPeerDescriptor()},
            {},
            true));
    }

    void TearDown() override {
        if (this->session) {
            this->session->stop();
        }
    }
};

TEST_F(RecursiveOperationSessionTest, HappyPath) {
    this->session = RecursiveOperationSession::newInstance(
        RecursiveOperationSessionOptions{
            .transport =
                *this->environment.createTransport(localPeerDescriptor),
            .targetId = Identifiers::createRandomDhtAddress(),
            .localPeerDescriptor = this->localPeerDescriptor,
            .waitedRoutingPathCompletions = waitedCompletions,
            .operation = RecursiveOperation::FIND_CLOSEST_NODES,
            .doRouteRequest =
                [this](const ::dht::RouteMessageWrapper& /*message*/) {
                    this->doRouteRequestCalled = true;
                    return RouteMessageAck{};
                }});

    this->session->start(ServiceID{""});
    EXPECT_TRUE(this->doRouteRequestCalled);
    for (size_t i = 0; i < responseCount; ++i) {
        this->sendResponseFrom(ServiceID{this->session->getId()});
    }

    blockingWait(waitForCondition(
        [this]() { return this->session->isCompletionEmitted(); }));
    const auto result = this->session->getResults();
    EXPECT_EQ(result.closestNodes.size(), responseCount * nodesPerResponse);
    EXPECT_TRUE(result.dataEntries.empty());
}
