// Ported from packages/dht/test/integration/DhtNodeRpcRemote.test.ts
// (v103.8.0-rc.3): two RpcCommunicators wired together, the server side
// answering ping / getClosestPeers, exercising DhtNodeRpcRemote's happy
// and error paths.
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.protos;
import streamr.dht.DhtRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::ClosestPeersRequest;
using ::dht::ClosestPeersResponse;
using ::dht::PeerDescriptor;
using ::dht::PingRequest;
using ::dht::PingResponse;
using ::protorpc::RpcMessage;
using streamr::dht::DhtNodeRpcRemote;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::protorpc::RpcCommunicator;
using streamr::utils::blockingWait;

using DhtNodeRpcClient = ::dht::DhtNodeRpcClient<DhtCallContext>;
using RpcCommunicatorType = RpcCommunicator<DhtCallContext>;

namespace {

constexpr auto serviceId = "test";
constexpr size_t mockPeerCount = 4;

std::vector<PeerDescriptor> createMockPeers() {
    std::vector<PeerDescriptor> peers;
    peers.reserve(mockPeerCount);
    for (size_t i = 0; i < mockPeerCount; ++i) {
        peers.push_back(createMockPeerDescriptor());
    }
    return peers;
}

} // namespace

class DhtNodeRpcRemoteTest : public ::testing::Test {
protected:
    RpcCommunicatorType clientCommunicator;
    RpcCommunicatorType serverCommunicator;
    PeerDescriptor clientPeerDescriptor = createMockPeerDescriptor();
    PeerDescriptor serverPeerDescriptor = createMockPeerDescriptor();
    std::vector<PeerDescriptor> mockPeers = createMockPeers();
    bool pingShouldThrow = false;
    bool getClosestPeersShouldThrow = false;
    std::optional<DhtNodeRpcRemote> rpcRemote;

    void SetUp() override {
        serverCommunicator
            .registerRpcMethod<ClosestPeersRequest, ClosestPeersResponse>(
                "getClosestPeers",
                [this](
                    const ClosestPeersRequest& request,
                    const DhtCallContext& /*context*/) -> ClosestPeersResponse {
                    if (this->getClosestPeersShouldThrow) {
                        throw std::runtime_error("Closest peers error");
                    }
                    ClosestPeersResponse response;
                    for (const auto& peer : this->mockPeers) {
                        *response.add_peers() = peer;
                    }
                    response.set_requestid(request.requestid());
                    return response;
                });
        serverCommunicator.registerRpcMethod<PingRequest, PingResponse>(
            "ping",
            [this](
                const PingRequest& request,
                const DhtCallContext& /*context*/) -> PingResponse {
                if (this->pingShouldThrow) {
                    throw std::runtime_error("Ping error");
                }
                PingResponse response;
                response.set_requestid(request.requestid());
                return response;
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
            ServiceID{serviceId},
            DhtNodeRpcClient(this->clientCommunicator));
    }
};

TEST_F(DhtNodeRpcRemoteTest, PingHappyPath) {
    const bool active = blockingWait(this->rpcRemote->ping());
    EXPECT_TRUE(active);
}

TEST_F(DhtNodeRpcRemoteTest, GetClosestPeersHappyPath) {
    const auto neighbors = blockingWait(this->rpcRemote->getClosestPeers(
        Identifiers::getNodeIdFromPeerDescriptor(clientPeerDescriptor)));
    EXPECT_EQ(neighbors.size(), mockPeers.size());
}

TEST_F(DhtNodeRpcRemoteTest, PingErrorPath) {
    this->pingShouldThrow = true;
    const bool active = blockingWait(this->rpcRemote->ping());
    EXPECT_FALSE(active);
}

TEST_F(DhtNodeRpcRemoteTest, GetClosestPeersErrorPath) {
    this->getClosestPeersShouldThrow = true;
    EXPECT_THROW(
        blockingWait(this->rpcRemote->getClosestPeers(
            Identifiers::getNodeIdFromPeerDescriptor(clientPeerDescriptor))),
        std::exception);
}
