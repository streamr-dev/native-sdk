// Ported from packages/dht/test/unit/StoreRpcLocal.test.ts
// (v103.8.0-rc.3): storeData marks entries stale unless this node stores the
// key; replicateData fans a newly stored entry out to the neighbouring
// storers (all of them if this node is the primary storer, else the closest
// one), never to the requestor or self.
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>
#include <gtest/gtest.h>

import streamr.dht.DhtCallContext;
import streamr.dht.getClosestNodes;
import streamr.dht.Identifiers;
import streamr.dht.LocalDataStore;
import streamr.dht.StoreRpcLocal;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using ::dht::ReplicateDataRequest;
using ::dht::StoreDataRequest;
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::store::LocalDataStore;
using streamr::dht::store::StoreRpcLocal;
using streamr::dht::store::StoreRpcLocalOptions;
using streamr::dht::testutils::createMockDataEntry;
using streamr::dht::testutils::createMockPeerDescriptor;

namespace {

constexpr size_t nodeCount = 5;

class MockLocalDataStore : public LocalDataStore {
public:
    bool storeEntryResult = true;
    int setAllEntriesAsStaleCount = 0;

    MockLocalDataStore() : LocalDataStore(0) {}

    bool storeEntry(const DataEntry& /*dataEntry*/) override {
        return this->storeEntryResult;
    }
    void setAllEntriesAsStale(const DhtAddress& /*key*/) override {
        ++this->setAllEntriesAsStaleCount;
    }
};

} // namespace

class StoreRpcLocalTest : public ::testing::Test {
protected:
    MockLocalDataStore localDataStore;
    DataEntry dataEntry = createMockDataEntry();
    std::vector<PeerDescriptor> allNodes = makeNodes();
    int replicateCount = 0;

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

    [[nodiscard]] std::unique_ptr<StoreRpcLocal> makeLocal(
        const PeerDescriptor& localNode, bool excludeLocalFromStorers) {
        return std::make_unique<StoreRpcLocal>(StoreRpcLocalOptions{
            .localDataStore = this->localDataStore,
            .localPeerDescriptor = localNode,
            .replicateDataToContact =
                [this](
                    const DataEntry& /*entry*/,
                    const PeerDescriptor& /*peer*/) { ++this->replicateCount; },
            .getStorers =
                [this, localNode, excludeLocalFromStorers](
                    const DhtAddress& key) {
                    auto storers = getClosestNodes(key, this->allNodes);
                    if (excludeLocalFromStorers) {
                        std::erase_if(
                            storers, [&localNode](const PeerDescriptor& peer) {
                                return Identifiers::areEqualPeerDescriptors(
                                    peer, localNode);
                            });
                    }
                    return storers;
                }});
    }

    [[nodiscard]] static DhtCallContext contextFromRandom() {
        DhtCallContext context;
        context.incomingSourceDescriptor = createMockPeerDescriptor();
        return context;
    }

    StoreDataRequest storeRequest() {
        StoreDataRequest request;
        request.set_key(this->dataEntry.key());
        return request;
    }

    ReplicateDataRequest replicateRequest() {
        ReplicateDataRequest request;
        *request.mutable_entry() = this->dataEntry;
        return request;
    }
};

TEST_F(StoreRpcLocalTest, PrimaryStorerStoreData) {
    auto local = makeLocal(nodeCloseToData(0), false);
    local->storeData(storeRequest(), DhtCallContext());
    EXPECT_EQ(this->localDataStore.setAllEntriesAsStaleCount, 0);
}

TEST_F(StoreRpcLocalTest, PrimaryStorerReplicateData) {
    auto local = makeLocal(nodeCloseToData(0), false);
    local->replicateData(
        replicateRequest(), StoreRpcLocalTest::contextFromRandom());
    EXPECT_EQ(this->localDataStore.setAllEntriesAsStaleCount, 0);
    EXPECT_EQ(this->replicateCount, static_cast<int>(nodeCount - 1));
}

TEST_F(StoreRpcLocalTest, StorerStoreData) {
    auto local = makeLocal(nodeCloseToData(1), false);
    local->storeData(storeRequest(), DhtCallContext());
    EXPECT_EQ(this->localDataStore.setAllEntriesAsStaleCount, 0);
}

TEST_F(StoreRpcLocalTest, StorerReplicateData) {
    auto local = makeLocal(nodeCloseToData(1), false);
    local->replicateData(
        replicateRequest(), StoreRpcLocalTest::contextFromRandom());
    EXPECT_EQ(this->localDataStore.setAllEntriesAsStaleCount, 0);
    EXPECT_EQ(this->replicateCount, 1);
}

TEST_F(StoreRpcLocalTest, NotStorerStoreData) {
    auto local = makeLocal(nodeCloseToData(nodeCount - 1), true);
    local->storeData(storeRequest(), DhtCallContext());
    EXPECT_GT(this->localDataStore.setAllEntriesAsStaleCount, 0);
}

TEST_F(StoreRpcLocalTest, NotStorerReplicateData) {
    auto local = makeLocal(nodeCloseToData(nodeCount - 1), true);
    local->replicateData(
        replicateRequest(), StoreRpcLocalTest::contextFromRandom());
    EXPECT_EQ(this->localDataStore.setAllEntriesAsStaleCount, 1);
    EXPECT_EQ(this->replicateCount, 1);
}

TEST_F(StoreRpcLocalTest, DataNotStoredReplicateData) {
    this->localDataStore.storeEntryResult = false;
    auto local = makeLocal(nodeCloseToData(1), false);
    local->replicateData(
        replicateRequest(), StoreRpcLocalTest::contextFromRandom());
    EXPECT_EQ(this->localDataStore.setAllEntriesAsStaleCount, 0);
    EXPECT_EQ(this->replicateCount, 0);
}
