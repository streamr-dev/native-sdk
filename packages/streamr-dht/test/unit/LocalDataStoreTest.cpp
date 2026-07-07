// Ported from packages/dht/test/unit/LocalDataStore.test.ts
// (v103.8.0-rc.3). Also validates the A5 MapWithTtl lazy-expiry adaptation:
// the "deleted after TTL" case waits real time and expects the entry gone on
// the next access.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <thread>
#include <vector>
#include <gtest/gtest.h>

import streamr.dht.Identifiers;
import streamr.dht.LocalDataStore;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::DataEntry;
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::dht::store::LocalDataStore;
using streamr::dht::testutils::createMockDataEntry;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::dht::testutils::MockDataEntryOptions;

namespace {

constexpr uint32_t maxTtl = 30000;

DhtAddress keyOf(const DataEntry& entry) {
    return Identifiers::getDhtAddressFromRaw(DhtAddressRaw{entry.key()});
}

DhtAddress creatorOf(const DataEntry& entry) {
    return Identifiers::getDhtAddressFromRaw(DhtAddressRaw{entry.creator()});
}

bool containsCreator(
    const std::vector<DataEntry>& entries, const DhtAddress& creator) {
    return std::ranges::any_of(entries, [&creator](const DataEntry& entry) {
        return creatorOf(entry) == creator;
    });
}

} // namespace

class LocalDataStoreTest : public ::testing::Test {
protected:
    LocalDataStore localDataStore{maxTtl};

    void TearDown() override { this->localDataStore.clear(); }
};

TEST_F(LocalDataStoreTest, CanStore) {
    const auto storedEntry = createMockDataEntry();
    this->localDataStore.storeEntry(storedEntry);
    const auto fetched = this->localDataStore.values(keyOf(storedEntry));
    ASSERT_EQ(fetched.size(), 1U);
    EXPECT_EQ(creatorOf(fetched[0]), creatorOf(storedEntry));
}

TEST_F(LocalDataStoreTest, MultipleStorersBehindOneKey) {
    const DhtAddress key = Identifiers::createRandomDhtAddress();
    const DhtAddress creator1 = Identifiers::createRandomDhtAddress();
    const DhtAddress creator2 = Identifiers::createRandomDhtAddress();
    this->localDataStore.storeEntry(createMockDataEntry(
        MockDataEntryOptions{.key = key, .creator = creator1}));
    this->localDataStore.storeEntry(createMockDataEntry(
        MockDataEntryOptions{.key = key, .creator = creator2}));
    const auto fetched = this->localDataStore.values(key);
    EXPECT_EQ(fetched.size(), 2U);
    EXPECT_TRUE(containsCreator(fetched, creator1));
    EXPECT_TRUE(containsCreator(fetched, creator2));
}

TEST_F(LocalDataStoreTest, CanRemoveDataEntries) {
    const DhtAddress key = Identifiers::createRandomDhtAddress();
    const DhtAddress creator1 = Identifiers::createRandomDhtAddress();
    const DhtAddress creator2 = Identifiers::createRandomDhtAddress();
    this->localDataStore.storeEntry(createMockDataEntry(
        MockDataEntryOptions{.key = key, .creator = creator1}));
    this->localDataStore.storeEntry(createMockDataEntry(
        MockDataEntryOptions{.key = key, .creator = creator2}));
    this->localDataStore.deleteEntry(key, creator1);
    const auto fetched = this->localDataStore.values(key);
    ASSERT_EQ(fetched.size(), 1U);
    EXPECT_EQ(creatorOf(fetched[0]), creator2);
}

TEST_F(LocalDataStoreTest, CanRemoveAllDataEntries) {
    const DhtAddress key = Identifiers::createRandomDhtAddress();
    const DhtAddress creator1 = Identifiers::createRandomDhtAddress();
    const DhtAddress creator2 = Identifiers::createRandomDhtAddress();
    this->localDataStore.storeEntry(createMockDataEntry(
        MockDataEntryOptions{.key = key, .creator = creator1}));
    this->localDataStore.storeEntry(createMockDataEntry(
        MockDataEntryOptions{.key = key, .creator = creator2}));
    this->localDataStore.deleteEntry(key, creator1);
    this->localDataStore.deleteEntry(key, creator2);
    EXPECT_TRUE(this->localDataStore.values(key).empty());
}

TEST_F(LocalDataStoreTest, DataIsDeletedAfterTtl) {
    constexpr uint32_t shortTtl = 1000;
    constexpr int ttlWaitMillis = 1100;
    const auto storedEntry =
        createMockDataEntry(MockDataEntryOptions{.ttl = shortTtl});
    this->localDataStore.storeEntry(storedEntry);
    EXPECT_EQ(this->localDataStore.values(keyOf(storedEntry)).size(), 1U);
    std::this_thread::sleep_for(std::chrono::milliseconds(ttlWaitMillis));
    EXPECT_TRUE(this->localDataStore.values(keyOf(storedEntry)).empty());
}

TEST_F(LocalDataStoreTest, MarkAsDeletedHappyPath) {
    const DhtAddress creator = Identifiers::createRandomDhtAddress();
    const auto storedEntry =
        createMockDataEntry(MockDataEntryOptions{.creator = creator});
    this->localDataStore.storeEntry(storedEntry);
    const auto notDeleted = this->localDataStore.values(keyOf(storedEntry));
    ASSERT_EQ(notDeleted.size(), 1U);
    EXPECT_FALSE(notDeleted[0].deleted());
    EXPECT_TRUE(
        this->localDataStore.markAsDeleted(keyOf(storedEntry), creator));
    const auto deleted = this->localDataStore.values(keyOf(storedEntry));
    ASSERT_EQ(deleted.size(), 1U);
    EXPECT_TRUE(deleted[0].deleted());
}

TEST_F(LocalDataStoreTest, MarkAsDeletedDataNotStored) {
    EXPECT_FALSE(this->localDataStore.markAsDeleted(
        Identifiers::createRandomDhtAddress(),
        Identifiers::getNodeIdFromPeerDescriptor(createMockPeerDescriptor())));
}

TEST_F(LocalDataStoreTest, MarkAsDeletedWrongCreator) {
    const auto storedEntry = createMockDataEntry();
    this->localDataStore.storeEntry(storedEntry);
    EXPECT_FALSE(this->localDataStore.markAsDeleted(
        keyOf(storedEntry),
        Identifiers::getNodeIdFromPeerDescriptor(createMockPeerDescriptor())));
}
