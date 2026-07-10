// Ported from packages/dht/test/integration/DhtNode.test.ts (v103.8.0-rc.3):
// a DhtNode over a FakeTransport joins a DHT whose other members are RPC
// stubs (a ListeningRpcCommunicator + DhtNodeRpcLocal each, answering ping
// and getClosestPeers with the full membership), and ends up with every
// other member as a neighbour and closest contact.
#include <chrono>
#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtNode;
import streamr.dht.DhtNodeRpcLocal;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.FakeTransport;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.ringIdentifiers;
import streamr.dht.TestUtils;
import streamr.dht.protos;

using ::dht::ClosestPeersRequest;
using ::dht::ClosestPeersResponse;
using ::dht::PeerDescriptor;
using ::dht::PingRequest;
using ::dht::PingResponse;
using streamr::dht::ClosestRingPeerDescriptors;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::DhtNodeRpcLocal;
using streamr::dht::DhtNodeRpcLocalOptions;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::contact::RingIdRaw;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::FakeTransport;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::utils::blockingWait;
using streamr::utils::waitForCondition;

namespace {

constexpr size_t otherNodeCount = 3;
constexpr auto serviceIdLayer0 = "layer0";
constexpr auto convergenceTimeout = std::chrono::seconds(10);
constexpr auto convergencePollInterval = std::chrono::milliseconds(100);

// The transport and communicator of one RPC-stub member (TS keeps them alive
// via closure capture; here the fixture owns them).
struct RemoteStubNode {
    std::shared_ptr<FakeTransport> transport;
    std::unique_ptr<ListeningRpcCommunicator> rpcCommunicator;
    std::unique_ptr<DhtNodeRpcLocal> dhtNodeRpcLocal;
};

} // namespace

class DhtNodeTest : public ::testing::Test {
protected:
    PeerDescriptor localPeerDescriptor;
    PeerDescriptor entryPointPeerDescriptor;
    std::vector<PeerDescriptor> otherPeerDescriptors;
    std::vector<std::unique_ptr<RemoteStubNode>> stubNodes;

    void SetUp() override {
        this->localPeerDescriptor = createMockPeerDescriptor();
        this->entryPointPeerDescriptor = createMockPeerDescriptor();
        for (size_t i = 0; i < otherNodeCount; i++) {
            this->otherPeerDescriptors.push_back(createMockPeerDescriptor());
        }
    }

    [[nodiscard]] std::vector<PeerDescriptor> getAllPeerDescriptors() const {
        std::vector<PeerDescriptor> all{
            this->localPeerDescriptor, this->entryPointPeerDescriptor};
        all.insert(
            all.end(),
            this->otherPeerDescriptors.begin(),
            this->otherPeerDescriptors.end());
        return all;
    }

    void startRemoteNode(
        const PeerDescriptor& peerDescriptor, FakeEnvironment& environment) {
        auto stub = std::make_unique<RemoteStubNode>();
        stub->transport = environment.createTransport(peerDescriptor);
        stub->rpcCommunicator = std::make_unique<ListeningRpcCommunicator>(
            ServiceID{serviceIdLayer0}, *stub->transport);
        stub->dhtNodeRpcLocal =
            std::make_unique<DhtNodeRpcLocal>(DhtNodeRpcLocalOptions{
                .peerDiscoveryQueryBatchSize = otherNodeCount + 2,
                .getNeighbors =
                    [this, peerDescriptor]() {
                        // without(getAllPeerDescriptors(), peerDescriptor)
                        std::vector<PeerDescriptor> neighbors;
                        for (const auto& descriptor :
                             this->getAllPeerDescriptors()) {
                            if (!Identifiers::areEqualPeerDescriptors(
                                    descriptor, peerDescriptor)) {
                                neighbors.push_back(descriptor);
                            }
                        }
                        return neighbors;
                    },
                // Never called in this test (no ring queries); TS passes
                // undefined.
                .getClosestRingContactsTo =
                    [](const RingIdRaw& /*ringIdRaw*/, size_t /*limit*/) {
                        return ClosestRingPeerDescriptors{};
                    },
                .addContact = [](const PeerDescriptor& /*contact*/) {},
                // Never called in this test; TS passes undefined.
                .removeContact = [](const streamr::dht::DhtAddress&
                                    /*nodeId*/) {}});
        auto* rpcLocal = stub->dhtNodeRpcLocal.get();
        stub->rpcCommunicator->registerRpcMethod<PingRequest, PingResponse>(
            "ping",
            [rpcLocal](const PingRequest& req, const DhtCallContext& context) {
                return rpcLocal->ping(req, context);
            });
        stub->rpcCommunicator
            ->registerRpcMethod<ClosestPeersRequest, ClosestPeersResponse>(
                "getClosestPeers",
                [rpcLocal](
                    const ClosestPeersRequest& req,
                    const DhtCallContext& context) {
                    return rpcLocal->getClosestPeers(req, context);
                });
        this->stubNodes.push_back(std::move(stub));
    }
};

TEST_F(DhtNodeTest, StartNodeAndJoinDht) {
    FakeEnvironment environment;
    this->startRemoteNode(this->entryPointPeerDescriptor, environment);
    for (const auto& other : this->otherPeerDescriptors) {
        this->startRemoteNode(other, environment);
    }

    auto transport = environment.createTransport(this->localPeerDescriptor);
    DhtNode localNode(
        DhtNodeOptions{
            .transport = transport.get(),
            .connectionsView = transport.get(),
            .peerDescriptor = this->localPeerDescriptor,
            .entryPoints = {this->entryPointPeerDescriptor}});
    blockingWait(localNode.start());
    blockingWait(localNode.joinDht({this->entryPointPeerDescriptor}));
    blockingWait(localNode.waitForNetworkConnectivity());

    blockingWait(waitForCondition(
        [&localNode]() {
            return localNode.getNeighborCount() == otherNodeCount + 1;
        },
        convergenceTimeout,
        convergencePollInterval));

    // toIncludeSameMembers(expected, actual) on node ids.
    std::set<std::string> expectedNodeIds;
    for (const auto& descriptor : this->getAllPeerDescriptors()) {
        if (!Identifiers::areEqualPeerDescriptors(
                descriptor, this->localPeerDescriptor)) {
            expectedNodeIds.insert(
                static_cast<std::string>(
                    Identifiers::getNodeIdFromPeerDescriptor(descriptor)));
        }
    }
    std::set<std::string> actualNodeIds;
    for (const auto& descriptor : localNode.getClosestContacts()) {
        actualNodeIds.insert(
            static_cast<std::string>(
                Identifiers::getNodeIdFromPeerDescriptor(descriptor)));
    }
    EXPECT_EQ(actualNodeIds, expectedNodeIds);

    localNode.stop();
}
