// Ported from packages/dht/test/unit/PeerManager.test.ts (v103.8.0-rc.3).
//
// The contacts are backed by a mock DhtNodeRpcRemote whose ping() result is
// driven by a shared 'pingFailures' set (a node whose id is in the set is
// treated as offline). The real RPC path is never exercised: a plain
// RpcCommunicator with a no-op outgoing callback stands in for the network,
// mirroring the TS MockRpcCommunicator (a RoutingRpcCommunicator with an
// empty send function). PeerManager pings newly added contacts off the
// calling thread (via AbortableTimers), so the async assertions wait on the
// resulting state with waitForCondition.
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;
import streamr.protorpc.RpcCommunicator;
import streamr.protorpc.protos;
import streamr.dht.DhtRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.PeerManager;
import streamr.dht.DhtNodeRpcRemote;
import streamr.dht.Identifiers;
import streamr.dht.getClosestNodes;
import streamr.dht.ConnectionLockStates;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::PeerDescriptor;
using ::protorpc::RpcMessage;
using streamr::dht::DhtAddress;
using streamr::dht::DhtNodeRpcRemote;
using streamr::dht::Identifiers;
using streamr::dht::PeerManager;
using streamr::dht::PeerManagerOptions;
using streamr::dht::ServiceID;
using streamr::dht::connection::LockID;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::testutils::assertEqualPeerDescriptors;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::protorpc::RpcCommunicator;
using streamr::utils::blockingWait;
using streamr::utils::waitForCondition;

using DhtNodeRpcClient = ::dht::DhtNodeRpcClient<DhtCallContext>;

namespace {

constexpr size_t numberOfNodesPerKBucket = 8;
constexpr size_t maxContactCount = 200;
constexpr size_t nearbyContactCount = 10;
constexpr size_t successContactCount = 5;

// The anonymous DhtNodeRpcRemote subclass from the TS test, which overrides
// ping() to consult a shared pingFailures set instead of hitting the network.
class MockDhtNodeRpcRemote : public DhtNodeRpcRemote {
private:
    std::shared_ptr<std::set<DhtAddress>> pingFailures;
    DhtAddress nodeId;

public:
    MockDhtNodeRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        DhtNodeRpcClient client,
        std::shared_ptr<std::set<DhtAddress>> pingFailures)
        : DhtNodeRpcRemote(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              ServiceID{"mock"},
              client),
          pingFailures(std::move(pingFailures)),
          nodeId(this->getNodeId()) {}

    folly::coro::Task<bool> ping() override {
        co_return !this->pingFailures->contains(this->nodeId);
    }
};

} // namespace

class PeerManagerTest : public ::testing::Test {
protected:
    RpcCommunicator<DhtCallContext> mockCommunicator;
    std::shared_ptr<std::set<DhtAddress>> pingFailures =
        std::make_shared<std::set<DhtAddress>>();
    PeerDescriptor localPeerDescriptor = createMockPeerDescriptor();
    std::shared_ptr<PeerManager> manager;

    void SetUp() override {
        this->mockCommunicator.setOutgoingMessageCallback(
            [](const RpcMessage& /*message*/,
               const std::string& /*requestId*/,
               const DhtCallContext& /*context*/) {});
    }

    void TearDown() override {
        if (this->manager) {
            this->manager->stop();
        }
    }

    PeerManagerOptions makeOptions() {
        return PeerManagerOptions{
            .numberOfNodesPerKBucket = numberOfNodesPerKBucket,
            .maxContactCount = maxContactCount,
            .localNodeId = Identifiers::getNodeIdFromPeerDescriptor(
                this->localPeerDescriptor),
            .localPeerDescriptor = this->localPeerDescriptor,
            .connectionLocker = nullptr,
            .neighborPingLimit = std::nullopt,
            .lockId = LockID{"peer-manager"},
            .createDhtNodeRpcRemote =
                [this](const PeerDescriptor& peerDescriptor)
                -> std::shared_ptr<DhtNodeRpcRemote> {
                return std::make_shared<MockDhtNodeRpcRemote>(
                    this->localPeerDescriptor,
                    peerDescriptor,
                    DhtNodeRpcClient(this->mockCommunicator),
                    this->pingFailures);
            },
            .hasConnection =
                [](const DhtAddress& /*nodeId*/) { return false; }};
    }

    std::shared_ptr<PeerManager> createPeerManager(
        const std::vector<PeerDescriptor>& contacts) {
        auto instance = PeerManager::newInstance(this->makeOptions());
        for (const auto& contact : contacts) {
            instance->addContact(contact);
        }
        return instance;
    }

    [[nodiscard]] bool neighborsContain(const DhtAddress& nodeId) {
        const auto neighbors = this->manager->getNeighbors();
        return std::ranges::any_of(
            neighbors, [&nodeId](const std::shared_ptr<DhtNodeRpcRemote>& n) {
                return n->getNodeId() == nodeId;
            });
    }
};

TEST_F(PeerManagerTest, GetNearbyContactCount) {
    std::vector<PeerDescriptor> contacts;
    std::vector<DhtAddress> nodeIds;
    for (size_t i = 0; i < nearbyContactCount; ++i) {
        const auto contact = createMockPeerDescriptor();
        nodeIds.push_back(Identifiers::getNodeIdFromPeerDescriptor(contact));
        contacts.push_back(contact);
    }
    this->manager = this->createPeerManager(contacts);

    EXPECT_EQ(this->manager->getNearbyContactCount(), nearbyContactCount);
    EXPECT_EQ(
        this->manager->getNearbyContactCount(
            std::set<DhtAddress>{nodeIds[0], nodeIds[1]}),
        nearbyContactCount - 2);
    EXPECT_EQ(
        this->manager->getNearbyContactCount(
            std::set<DhtAddress>{
                nodeIds[0], Identifiers::createRandomDhtAddress()}),
        nearbyContactCount - 1);
}

TEST_F(PeerManagerTest, AddContactPingFails) {
    std::vector<PeerDescriptor> successContacts;
    successContacts.reserve(successContactCount);
    for (size_t i = 0; i < successContactCount; ++i) {
        successContacts.push_back(createMockPeerDescriptor());
    }
    const auto failureContact = createMockPeerDescriptor();
    this->pingFailures->insert(
        Identifiers::getNodeIdFromPeerDescriptor(failureContact));

    this->manager = this->createPeerManager({});
    for (const auto& successContact : successContacts) {
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(successContact);
        this->manager->addContact(successContact);
        this->manager->setContactActive(nodeId);
        this->manager->removeNeighbor(nodeId);
    }
    EXPECT_EQ(this->manager->getNeighborCount(), 0U);

    this->manager->addContact(failureContact);
    const auto closestSuccessContact = getClosestNodes(
        Identifiers::getNodeIdFromPeerDescriptor(this->localPeerDescriptor),
        successContacts)[0];
    const auto closestNodeId =
        Identifiers::getNodeIdFromPeerDescriptor(closestSuccessContact);
    blockingWait(waitForCondition([this, &closestNodeId]() {
        return this->neighborsContain(closestNodeId);
    }));

    EXPECT_EQ(this->manager->getNeighborCount(), 1U);
    EXPECT_PRED_FORMAT2(
        assertEqualPeerDescriptors,
        closestSuccessContact,
        this->manager->getNeighbors()[0]->getPeerDescriptor());
}

TEST_F(PeerManagerTest, PruneOfflineNodesRemovesOfflineNodes) {
    std::vector<PeerDescriptor> successContacts;
    successContacts.reserve(successContactCount);
    for (size_t i = 0; i < successContactCount; ++i) {
        successContacts.push_back(createMockPeerDescriptor());
    }
    const auto failureContact = createMockPeerDescriptor();

    this->manager = this->createPeerManager({});
    for (const auto& successContact : successContacts) {
        this->manager->addContact(successContact);
    }
    this->manager->addContact(failureContact);
    EXPECT_EQ(this->manager->getNeighborCount(), successContactCount + 1);

    this->pingFailures->insert(
        Identifiers::getNodeIdFromPeerDescriptor(failureContact));
    std::vector<std::shared_ptr<DhtNodeRpcRemote>> nodes;
    for (const auto& neighbor : this->manager->getNeighbors()) {
        nodes.push_back(
            std::make_shared<MockDhtNodeRpcRemote>(
                this->localPeerDescriptor,
                neighbor->getPeerDescriptor(),
                DhtNodeRpcClient(this->mockCommunicator),
                this->pingFailures));
    }
    blockingWait(this->manager->pruneOfflineNodes(nodes));

    EXPECT_EQ(this->manager->getNeighborCount(), successContactCount);
}
