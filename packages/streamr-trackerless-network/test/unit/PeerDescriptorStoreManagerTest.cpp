// Ported from packages/trackerless-network/test/unit/
// PeerDescriptorStoreManager.test.ts (v103.8.0-rc.3). The DHT operations
// are fakes counting store calls; the last case is timing-based like the
// TS original (storeInterval 2 s, wait 4.5 s → two extra keep-alive
// stores).
// The generated dht protos come ONLY from `import streamr.dht.protos` — a
// textual DhtRpc.pb.h include alongside the BMI makes clangd flag every
// member call on those types as ambiguous.
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.trackerlessnetwork.PeerDescriptorStoreManager;
import streamr.trackerlessnetwork.TestUtils;
import streamr.utils.CoroutineHelper;

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::trackerlessnetwork::controllayer::PeerDescriptorStoreManager;
using streamr::trackerlessnetwork::controllayer::
    PeerDescriptorStoreManagerOptions;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::utils::blockingWait;

namespace {
constexpr std::chrono::milliseconds testStoreInterval{2000};
// TS waits 4.5 s: two keep-alive ticks of the 2 s interval fit.
constexpr std::chrono::milliseconds keepAliveObservationTime{4500};
constexpr uint32_t fakeDataTtlMs = 1000;
} // namespace

class PeerDescriptorStoreManagerTest : public ::testing::Test {
protected:
    PeerDescriptor peerDescriptor = createMockPeerDescriptor();
    PeerDescriptor deletedPeerDescriptor = createMockPeerDescriptor();
    std::atomic<int> storeCalled = 0;
    std::unique_ptr<PeerDescriptorStoreManager> withData;
    std::unique_ptr<PeerDescriptorStoreManager> withoutData;

    [[nodiscard]] static DataEntry createFakeData(
        const PeerDescriptor& descriptor, bool deleted) {
        DataEntry entry;
        entry.set_key("\x01\x02\x03");
        entry.mutable_data()->PackFrom(descriptor);
        entry.set_creator(descriptor.nodeid());
        entry.set_ttl(fakeDataTtlMs);
        entry.set_stale(false);
        entry.set_deleted(deleted);
        return entry;
    }

    void SetUp() override {
        const auto key = Identifiers::createRandomDhtAddress();
        auto storeDataToDht = [this](auto /*key*/, auto /*data*/)
            -> folly::coro::Task<std::vector<PeerDescriptor>> {
            this->storeCalled++;
            co_return std::vector<PeerDescriptor>{this->peerDescriptor};
        };
        auto deleteDataFromDht = [](auto /*key*/, bool /*waitForCompletion*/)
            -> folly::coro::Task<void> { co_return; };
        this->withData = std::make_unique<PeerDescriptorStoreManager>(
            PeerDescriptorStoreManagerOptions{
                .key = key,
                .localPeerDescriptor = this->peerDescriptor,
                .storeInterval = testStoreInterval,
                .fetchDataFromDht = [this](auto /*key*/)
                    -> folly::coro::Task<std::vector<DataEntry>> {
                    co_return std::vector<DataEntry>{
                        createFakeData(this->peerDescriptor, false),
                        createFakeData(this->deletedPeerDescriptor, true)};
                },
                .storeDataToDht = storeDataToDht,
                .deleteDataFromDht = deleteDataFromDht});
        this->withoutData = std::make_unique<PeerDescriptorStoreManager>(
            PeerDescriptorStoreManagerOptions{
                .key = key,
                .localPeerDescriptor = this->peerDescriptor,
                .storeInterval = testStoreInterval,
                .fetchDataFromDht = [](auto /*key*/)
                    -> folly::coro::Task<std::vector<DataEntry>> {
                    co_return std::vector<DataEntry>{};
                },
                .storeDataToDht = storeDataToDht,
                .deleteDataFromDht = deleteDataFromDht});
    }

    void TearDown() override { blockingWait(this->withData->destroy()); }
};

TEST_F(PeerDescriptorStoreManagerTest, FetchNodesFiltersDeletedData) {
    const auto result = blockingWait(this->withData->fetchNodes());
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(
        Identifiers::areEqualPeerDescriptors(result[0], this->peerDescriptor),
        true);
}

TEST_F(PeerDescriptorStoreManagerTest, FetchNodesWithoutResults) {
    const auto result = blockingWait(this->withoutData->fetchNodes());
    EXPECT_EQ(result.size(), 0);
}

TEST_F(PeerDescriptorStoreManagerTest, StoreOnStreamWithoutSaturatedCount) {
    blockingWait(this->withData->storeAndKeepLocalNode());
    EXPECT_EQ(this->storeCalled, 1);
    EXPECT_EQ(this->withData->isLocalNodeStored(), true);
}

TEST_F(PeerDescriptorStoreManagerTest, WillKeepStoredUntilDestroyed) {
    blockingWait(this->withData->storeAndKeepLocalNode());
    EXPECT_EQ(this->storeCalled, 1);
    EXPECT_EQ(this->withData->isLocalNodeStored(), true);
    // storeInterval is 2 s: after 4.5 s the keep-alive loop has stored two
    // more times (TS waits the same 4.5 s).
    std::this_thread::sleep_for(keepAliveObservationTime);
    blockingWait(this->withData->destroy());
    EXPECT_EQ(this->storeCalled, 3);
}
