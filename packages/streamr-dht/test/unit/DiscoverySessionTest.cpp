// Ported from packages/dht/test/unit/DiscoverySession.test.ts
// (v103.8.0-rc.3): builds a connected 40-node test topology, then runs one
// DiscoverySession from a random node toward a random target with
// parallelism 1 and noProgressLimit 1, and checks that the sequence of queried
// nodes moves monotonically closer to the target.
//
// The mock DhtNodeRpcRemote (a subclass, since getClosestPeers/ping are
// virtual) answers getClosestPeers by rebuilding that node's PeerManager from
// the topology and returning its closest neighbours — exactly as the TS
// Partial<DhtNodeRpcRemote> mock does. createTestTopology is ported here from
// test/utils/topology.ts.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.protos;
import streamr.dht.ConnectionLockStates;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.DhtRpcClient;
import streamr.dht.DiscoverySession;
import streamr.dht.getClosestNodes;
import streamr.dht.getPeerDistance;
import streamr.dht.Identifiers;
import streamr.dht.PeerManager;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::DhtNodeRpcRemote;
using streamr::dht::Identifiers;
using streamr::dht::PeerManager;
using streamr::dht::PeerManagerOptions;
using streamr::dht::ServiceID;
using streamr::dht::connection::LockID;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::contact::GetClosestNodesOptions;
using streamr::dht::discovery::DiscoverySession;
using streamr::dht::discovery::DiscoverySessionOptions;
using streamr::dht::helpers::getPeerDistance;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::protorpc::RpcCommunicator;
using streamr::utils::AbortController;
using streamr::utils::blockingWait;

using DhtNodeRpcClient = ::dht::DhtNodeRpcClient<DhtCallContext>;

namespace {

constexpr size_t nodeCount = 40;
constexpr size_t minNeighborCount = 2;
constexpr size_t parallelism = 1;
constexpr size_t noProgressLimit = 1;
constexpr size_t queryBatchSize = 5;
constexpr size_t numberOfNodesPerKBucket = 8;
constexpr size_t maxContactCount = 200;
constexpr std::chrono::milliseconds queryDelay{10};
constexpr std::chrono::milliseconds discoverySessionTimeout{1000};

// A Multimap<DhtAddress, DhtAddress> from the TS test: node id -> its
// neighbours (a set, so edges are naturally deduplicated).
using Topology = std::map<DhtAddress, std::set<DhtAddress>>;

PeerDescriptor createPeerDescriptor(const DhtAddress& nodeId) {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(Identifiers::getRawFromDhtAddress(nodeId));
    descriptor.set_type(NodeType::NODEJS);
    return descriptor;
}

// The `count` node ids closest to referenceId, excluding referenceId itself
// (TS's local getClosestNodes with allowToContainReferenceId=false).
std::vector<DhtAddress> getClosestNodeIds(
    const DhtAddress& referenceId,
    const std::vector<DhtAddress>& candidates,
    size_t count) {
    std::vector<PeerDescriptor> descriptors;
    descriptors.reserve(candidates.size());
    for (const auto& id : candidates) {
        descriptors.push_back(createPeerDescriptor(id));
    }
    const auto closest = getClosestNodes(
        referenceId,
        descriptors,
        GetClosestNodesOptions{
            .maxCount = count,
            .excludedNodeIds = std::set<DhtAddress>{referenceId}});
    std::vector<DhtAddress> result;
    result.reserve(closest.size());
    for (const auto& descriptor : closest) {
        result.push_back(Identifiers::getNodeIdFromPeerDescriptor(descriptor));
    }
    return result;
}

// The connected components of the topology (TS getTopologyPartitions). A
// std::list keeps partition pointers stable across the merge/erase.
std::vector<std::set<DhtAddress>> getTopologyPartitions(
    const Topology& topology) {
    std::list<std::set<DhtAddress>> partitions;
    const auto findPartition = [&partitions](const DhtAddress& nodeId) {
        return std::ranges::find_if(
            partitions, [&nodeId](const std::set<DhtAddress>& partition) {
                return partition.contains(nodeId);
            });
    };
    for (const auto& [nodeId, neighbors] : topology) {
        const auto existing = findPartition(nodeId);
        if (existing != partitions.end()) {
            for (const auto& neighbor : neighbors) {
                if (existing->contains(neighbor)) {
                    continue;
                }
                const auto other = findPartition(neighbor);
                if (other != partitions.end()) {
                    existing->insert(other->begin(), other->end());
                    partitions.erase(other);
                } else {
                    existing->insert(neighbor);
                }
            }
        } else {
            std::set<DhtAddress> partition{nodeId};
            partition.insert(neighbors.begin(), neighbors.end());
            partitions.push_back(std::move(partition));
        }
    }
    return {partitions.begin(), partitions.end()};
}

// Builds a topology with no network splits, where each node's neighbours are
// globally closest to it (TS createTestTopology).
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Topology createTestTopology(size_t count, size_t minNeighbors) {
    std::vector<DhtAddress> nodeIds;
    nodeIds.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        nodeIds.push_back(Identifiers::createRandomDhtAddress());
    }
    Topology topology;
    for (const auto& nodeId : nodeIds) {
        for (const auto& closestNode :
             getClosestNodeIds(nodeId, nodeIds, minNeighbors)) {
            topology[nodeId].insert(closestNode);
            topology[closestNode].insert(nodeId);
        }
    }
    // Merge partitions by repeatedly joining the globally closest cross-
    // partition pair until a single component remains.
    while (true) {
        const auto partitions = getTopologyPartitions(topology);
        if (partitions.size() <= 1) {
            break;
        }
        std::optional<DhtAddress> mergeFrom;
        std::optional<DhtAddress> mergeTo;
        double bestDistance = 0;
        for (const auto& nodeId : nodeIds) {
            const auto& ownPartition = *std::ranges::find_if(
                partitions, [&nodeId](const std::set<DhtAddress>& partition) {
                    return partition.contains(nodeId);
                });
            std::vector<DhtAddress> otherNodes;
            for (const auto& candidate : nodeIds) {
                if (!ownPartition.contains(candidate)) {
                    otherNodes.push_back(candidate);
                }
            }
            const auto closestOther = getClosestNodeIds(nodeId, otherNodes, 1);
            if (closestOther.empty()) {
                continue;
            }
            const double distance = getPeerDistance(
                Identifiers::getRawFromDhtAddress(nodeId),
                Identifiers::getRawFromDhtAddress(closestOther.front()));
            if (!mergeFrom.has_value() || distance < bestDistance) {
                bestDistance = distance;
                mergeFrom = nodeId;
                mergeTo = closestOther.front();
            }
        }
        topology[*mergeFrom].insert(*mergeTo);
        topology[*mergeTo].insert(*mergeFrom);
    }
    return topology;
}

} // namespace

class DiscoverySessionTest : public ::testing::Test {
protected:
    RpcCommunicator<DhtCallContext> mockCommunicator;
    Topology topology;
    std::shared_ptr<std::vector<DhtAddress>> queriedNodes =
        std::make_shared<std::vector<DhtAddress>>();

    void SetUp() override {
        this->mockCommunicator.setOutgoingMessageCallback(
            [](const ::protorpc::RpcMessage& /*message*/,
               const std::string& /*requestId*/,
               const DhtCallContext& /*context*/) {});
        this->topology = createTestTopology(nodeCount, minNeighborCount);
    }

    // The mock: getClosestPeers rebuilds this node's PeerManager and returns
    // its closest neighbours; ping never touches the network.
    class MockDhtNodeRpcRemote : public DhtNodeRpcRemote {
    private:
        std::shared_ptr<std::vector<DhtAddress>> queriedNodes;
        std::function<std::vector<PeerDescriptor>(const DhtAddress&)>
            getClosest;

    public:
        MockDhtNodeRpcRemote(
            PeerDescriptor localPeerDescriptor, // NOLINT
            PeerDescriptor remotePeerDescriptor,
            DhtNodeRpcClient client,
            std::shared_ptr<std::vector<DhtAddress>> queriedNodes,
            std::function<std::vector<PeerDescriptor>(const DhtAddress&)>
                getClosest)
            : DhtNodeRpcRemote(
                  std::move(localPeerDescriptor),
                  std::move(remotePeerDescriptor),
                  ServiceID{"mock"},
                  client),
              queriedNodes(std::move(queriedNodes)),
              getClosest(std::move(getClosest)) {}

        folly::coro::Task<std::vector<PeerDescriptor>> getClosestPeers(
            const DhtAddress& referenceId) override {
            this->queriedNodes->push_back(this->getNodeId());
            co_await folly::coro::sleep(queryDelay);
            co_return this->getClosest(referenceId);
        }

        folly::coro::Task<bool> ping() override { co_return true; }
    };

    std::shared_ptr<DhtNodeRpcRemote> createMockRpcRemote(
        const PeerDescriptor& peerDescriptor) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor);
        // The callback rebuilds a PeerManager and may throw (allocation); it
        // runs inside the getClosestPeers coroutine, which handles exceptions,
        // and in this test any throw simply fails the test.
        // NOLINTBEGIN(bugprone-exception-escape)
        auto getClosest =
            [this, nodeId](
                const DhtAddress& referenceId) -> std::vector<PeerDescriptor> {
            const auto peerManager = this->createPeerManager(nodeId);
            std::vector<PeerDescriptor> neighborDescriptors;
            for (const auto& neighbor : peerManager->getNeighbors()) {
                neighborDescriptors.push_back(neighbor->getPeerDescriptor());
            }
            auto result = getClosestNodes(
                referenceId,
                neighborDescriptors,
                GetClosestNodesOptions{.maxCount = queryBatchSize});
            peerManager->stop();
            return result;
        };
        // NOLINTEND(bugprone-exception-escape)
        return std::make_shared<MockDhtNodeRpcRemote>(
            createMockPeerDescriptor(),
            peerDescriptor,
            DhtNodeRpcClient(this->mockCommunicator),
            this->queriedNodes,
            std::move(getClosest));
    }

    std::shared_ptr<PeerManager> createPeerManager(
        const DhtAddress& localNodeId) {
        auto peerManager = PeerManager::newInstance(
            PeerManagerOptions{
                .numberOfNodesPerKBucket = numberOfNodesPerKBucket,
                .maxContactCount = maxContactCount,
                .localNodeId = localNodeId,
                .localPeerDescriptor = createPeerDescriptor(localNodeId),
                .connectionLocker = nullptr,
                .neighborPingLimit = std::nullopt,
                .lockId = LockID{"discovery-test"},
                .createDhtNodeRpcRemote =
                    [this](const PeerDescriptor& peerDescriptor) {
                        return this->createMockRpcRemote(peerDescriptor);
                    },
                .hasConnection =
                    [](const DhtAddress& /*nodeId*/) { return true; }});
        for (const auto& neighbor : this->topology[localNodeId]) {
            peerManager->addContact(createPeerDescriptor(neighbor));
        }
        return peerManager;
    }
};

TEST_F(DiscoverySessionTest, HappyPath) {
    std::vector<DhtAddress> nodeIds;
    nodeIds.reserve(this->topology.size());
    for (const auto& [nodeId, neighbors] : this->topology) {
        nodeIds.push_back(nodeId);
    }
    ASSERT_GE(nodeIds.size(), 2U);
    const auto localNodeId = nodeIds.front();
    const auto targetId = nodeIds.at(nodeIds.size() / 2);

    std::set<DhtAddress> contactedPeers;
    const auto peerManager = this->createPeerManager(localNodeId);
    AbortController abortController;
    DiscoverySession session(
        DiscoverySessionOptions{
            .targetId = targetId,
            .parallelism = parallelism,
            .noProgressLimit = noProgressLimit,
            .peerManager = *peerManager,
            .contactedPeers = contactedPeers,
            .abortSignal = abortController.getSignal(),
            .createDhtNodeRpcRemote =
                [this](const PeerDescriptor& peerDescriptor) {
                    return this->createMockRpcRemote(peerDescriptor);
                }});

    blockingWait(session.findClosestNodes(discoverySessionTimeout));

    ASSERT_FALSE(this->queriedNodes->empty());
    // Each queried node is strictly closer to the target than the previous one
    // (parallelism 1, noProgressLimit 1 make the walk monotonic).
    const auto targetRaw = Identifiers::getRawFromDhtAddress(targetId);
    std::vector<double> distances;
    distances.reserve(this->queriedNodes->size());
    for (const auto& queried : *this->queriedNodes) {
        distances.push_back(getPeerDistance(
            Identifiers::getRawFromDhtAddress(queried), targetRaw));
    }
    for (size_t i = 1; i < distances.size(); ++i) {
        EXPECT_LT(distances[i], distances[i - 1]);
    }
    peerManager->stop();
}
