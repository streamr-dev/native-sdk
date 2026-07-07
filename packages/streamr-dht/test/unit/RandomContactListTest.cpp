// Ported from packages/dht/test/unit/RandomContactList.test.ts
// (v103.8.0-rc.3). randomness is 1 so every distinct contact is admitted,
// making the tests deterministic.
#include <memory>
#include <string>
#include <vector>
#include <gtest/gtest.h>

import streamr.dht.Identifiers;
import streamr.dht.RandomContactList;

using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::dht::contact::RandomContactList;

namespace {

constexpr size_t listMaxSize = 5;
constexpr double alwaysAdmit = 1.0;

struct TestItem {
    DhtAddress nodeId;
    explicit TestItem(const DhtAddressRaw& raw)
        : nodeId(Identifiers::getDhtAddressFromRaw(raw)) {}
    [[nodiscard]] DhtAddress getNodeId() const { return this->nodeId; }
};

std::shared_ptr<TestItem> createItem(const std::vector<unsigned char>& bytes) {
    return std::make_shared<TestItem>(
        DhtAddressRaw{std::string(bytes.begin(), bytes.end())});
}

std::vector<DhtAddress> nodeIdsOf(
    const std::vector<std::shared_ptr<TestItem>>& items) {
    std::vector<DhtAddress> ids;
    ids.reserve(items.size());
    for (const auto& item : items) {
        ids.push_back(item->getNodeId());
    }
    return ids;
}

} // namespace

class RandomContactListTest : public ::testing::Test {
protected:
    std::shared_ptr<TestItem> item0 = createItem({0, 0, 0, 0});
    std::shared_ptr<TestItem> item1 = createItem({0, 0, 0, 1});
    std::shared_ptr<TestItem> item2 = createItem({0, 0, 0, 2});
    std::shared_ptr<TestItem> item3 = createItem({0, 0, 0, 3});
    std::shared_ptr<TestItem> item4 = createItem({0, 0, 0, 4});
};

TEST_F(RandomContactListTest, AddsContactsCorrectly) {
    RandomContactList<TestItem> list(
        item0->getNodeId(), listMaxSize, alwaysAdmit);
    list.addContact(item1);
    list.addContact(item2);
    list.addContact(item3);
    list.addContact(item3);
    list.addContact(item4);
    list.addContact(item4);
    EXPECT_EQ(list.getSize(), 4U);
    EXPECT_EQ(
        nodeIdsOf(list.getContacts()), nodeIdsOf({item1, item2, item3, item4}));
}

TEST_F(RandomContactListTest, RemovesContactsCorrectly) {
    RandomContactList<TestItem> list(
        item0->getNodeId(), listMaxSize, alwaysAdmit);
    list.addContact(item1);
    list.addContact(item2);
    list.addContact(item3);
    list.addContact(item4);
    list.removeContact(item2->getNodeId());
    EXPECT_TRUE(list.getContact(item1->getNodeId()));
    EXPECT_TRUE(list.getContact(item3->getNodeId()));
    EXPECT_TRUE(list.getContact(item4->getNodeId()));
    EXPECT_EQ(nodeIdsOf(list.getContacts()), nodeIdsOf({item1, item3, item4}));
    EXPECT_EQ(list.getSize(), 3U);
}

TEST_F(RandomContactListTest, GetContacts) {
    RandomContactList<TestItem> list(
        item0->getNodeId(), listMaxSize, alwaysAdmit);
    list.addContact(item1);
    list.addContact(item2);
    list.addContact(item3);
    list.addContact(item4);
    EXPECT_EQ(list.getContacts().size(), 4U);
    EXPECT_EQ(list.getContacts(3).size(), 3U);
    EXPECT_EQ(list.getContacts(-2).size(), 0U);
}
