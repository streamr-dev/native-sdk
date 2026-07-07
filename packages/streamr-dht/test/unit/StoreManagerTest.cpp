// Ported from packages/dht/test/unit/StoreManager.test.ts (v103.8.0-rc.3):
// onContactAdded replicates data to a newly-added node when this node is the
// primary storer and the new node is within the redundancy factor, and marks
// entries stale when this node drops out of the storer set.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;
import streamr.protorpc.RpcCommunicator;
import streamr.dht.DhtCallContext;
import streamr.dht.DhtRpcClient;
import streamr.dht.getClosestNodes;
import streamr.dht.Identifiers;
import streamr.dht.LocalDataStore;
import streamr.dht.RecursiveOperationSession;
import streamr.dht.StoreManager;
import streamr.dht.StoreRpcRemote;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperation;
using ::dht::ReplicateDataRequest;
using ::dht::StoreDataRequest;
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::recursiveoperation::RecursiveOperationResult;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::store::LocalDataStore;
using streamr::dht::store::StoreManager;
using streamr::dht::store::StoreManagerOptions;
using streamr::dht::store::StoreRpcRemote;
using streamr::dht::testutils::createMockDataEntry;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::protorpc::RpcCommunicator;
using streamr::utils::blockingWait;
using streamr::utils::waitForCondition;

using StoreRpcClient = ::dht::StoreRpcClient<DhtCallContext>;

namespace {

constexpr size_t nodeCount = 10;
constexpr size_t redundancyFactor = 3;
constexpr size_t largeRedundancyFactor = 100;
constexpr int replicationWaitMillis = 50;
constexpr uint32_t defaultMaxTtl = 30000;

// Returns a fixed key/entry and records setAllEntriesAsStale calls.
class MockLocalDataStore : public LocalDataStore {
    DataEntry entry;
    DhtAddress key;

public:
    int setAllEntriesAsStaleCount = 0;

    MockLocalDataStore(DataEntry entry, DhtAddress key)
        : LocalDataStore(defaultMaxTtl),
          entry(std::move(entry)),
          key(std::move(key)) {}

    std::vector<DhtAddress> keys() override { return {this->key}; }
    std::vector<DataEntry> values(
        const std::optional<DhtAddress>& /*key*/) override {
        return {this->entry};
    }
    void setAllEntriesAsStale(const DhtAddress& /*key*/) override {
        ++this->setAllEntriesAsStaleCount;
    }
};

class MockStoreRpcRemote : public StoreRpcRemote {
    std::atomic<int>* replicateCount;
    std::atomic<bool>* lastConnect;

public:
    MockStoreRpcRemote(
        PeerDescriptor local, // NOLINT
        PeerDescriptor remote,
        StoreRpcClient client,
        std::atomic<int>* replicateCount,
        std::atomic<bool>* lastConnect)
        : StoreRpcRemote(std::move(local), std::move(remote), client),
          replicateCount(replicateCount),
          lastConnect(lastConnect) {}

    folly::coro::Task<void> replicateData(
        ReplicateDataRequest /*request*/, bool connect) override {
        this->lastConnect->store(connect);
        this->replicateCount->fetch_add(1);
        co_return;
    }
    folly::coro::Task<void> storeData(StoreDataRequest /*request*/) override {
        co_return;
    }
};

} // namespace

class StoreManagerTest : public ::testing::Test {
protected:
    RpcCommunicator<DhtCallContext> rpcCommunicator;
    DataEntry dataEntry = createMockDataEntry();
    std::vector<PeerDescriptor> allNodes = makeNodes();
    std::atomic<int> replicateCount{0};
    std::atomic<bool> lastConnect{false};
    std::shared_ptr<MockLocalDataStore> localDataStore;
    std::shared_ptr<StoreManager> manager;

    static std::vector<PeerDescriptor> makeNodes() {
        std::vector<PeerDescriptor> nodes;
        nodes.reserve(nodeCount);
        for (size_t i = 0; i < nodeCount; ++i) {
            nodes.push_back(createMockPeerDescriptor());
        }
        return nodes;
    }

    [[nodiscard]] DhtAddress dataKey() const {
        return Identifiers::getDhtAddressFromRaw(
            DhtAddressRaw{dataEntry.key()});
    }

    [[nodiscard]] PeerDescriptor nodeCloseToData(size_t index) {
        return getClosestNodes(this->dataKey(), this->allNodes)[index];
    }

    void makeManager(const PeerDescriptor& localNode, size_t redundancy) {
        this->localDataStore = std::make_shared<MockLocalDataStore>(
            this->dataEntry, this->dataKey());
        std::vector<PeerDescriptor> neighbors;
        for (const auto& node : this->allNodes) {
            if (!Identifiers::areEqualPeerDescriptors(node, localNode)) {
                neighbors.push_back(node);
            }
        }
        this->manager = StoreManager::newInstance(
            StoreManagerOptions{
                .rpcCommunicator = this->rpcCommunicator,
                .localPeerDescriptor = localNode,
                .localDataStore = *this->localDataStore,
                .serviceId = ServiceID{"StoreManager"},
                .highestTtl = defaultMaxTtl,
                .redundancyFactor = redundancy,
                .getNeighbors = [neighbors]() { return neighbors; },
                .createRpcRemote = [this,
                                    localNode](const PeerDescriptor& contact)
                    -> std::shared_ptr<StoreRpcRemote> {
                    return std::make_shared<MockStoreRpcRemote>(
                        localNode,
                        contact,
                        StoreRpcClient(this->rpcCommunicator),
                        &this->replicateCount,
                        &this->lastConnect);
                },
                .executeRecursiveOperation =
                    [](const DhtAddress& /*key*/,
                       RecursiveOperation /*operation*/)
                    -> folly::coro::Task<RecursiveOperationResult> {
                    co_return RecursiveOperationResult{};
                }});
    }

    void TearDown() override {
        if (this->manager) {
            blockingWait(this->manager->destroy());
        }
    }
};

TEST_F(StoreManagerTest, PrimaryStorerNewNodeWithinRedundancy) {
    makeManager(nodeCloseToData(0), redundancyFactor);
    this->manager->onContactAdded(nodeCloseToData(2));
    blockingWait(waitForCondition(
        [this]() { return this->replicateCount.load() == 1; }));
    EXPECT_TRUE(this->lastConnect.load());
    EXPECT_EQ(this->localDataStore->setAllEntriesAsStaleCount, 0);
}

TEST_F(StoreManagerTest, PrimaryStorerNewNodeNotWithinRedundancy) {
    makeManager(nodeCloseToData(0), redundancyFactor);
    this->manager->onContactAdded(nodeCloseToData(4));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(replicationWaitMillis));
    EXPECT_EQ(this->replicateCount.load(), 0);
    EXPECT_EQ(this->localDataStore->setAllEntriesAsStaleCount, 0);
}

TEST_F(StoreManagerTest, NotPrimaryStorerThisNodeWithinRedundancy) {
    makeManager(nodeCloseToData(1), redundancyFactor);
    this->manager->onContactAdded(nodeCloseToData(4));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(replicationWaitMillis));
    EXPECT_EQ(this->replicateCount.load(), 0);
    EXPECT_EQ(this->localDataStore->setAllEntriesAsStaleCount, 0);
}

TEST_F(StoreManagerTest, NotPrimaryStorerThisNodeNotWithinRedundancy) {
    makeManager(nodeCloseToData(3), redundancyFactor);
    this->manager->onContactAdded(nodeCloseToData(4));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(replicationWaitMillis));
    EXPECT_EQ(this->replicateCount.load(), 0);
    EXPECT_EQ(this->localDataStore->setAllEntriesAsStaleCount, 1);
}

TEST_F(StoreManagerTest, NotPrimaryWithinRedundancyFewNeighbors) {
    makeManager(nodeCloseToData(3), largeRedundancyFactor);
    this->manager->onContactAdded(nodeCloseToData(4));
    std::this_thread::sleep_for(
        std::chrono::milliseconds(replicationWaitMillis));
    EXPECT_EQ(this->replicateCount.load(), 0);
    EXPECT_EQ(this->localDataStore->setAllEntriesAsStaleCount, 0);
}
