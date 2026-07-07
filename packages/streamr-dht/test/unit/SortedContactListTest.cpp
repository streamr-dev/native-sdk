// Ported from packages/dht/test/unit/SortedContactList.test.ts
// (v103.8.0-rc.3).
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <gtest/gtest.h>

import streamr.dht.ContactList;
import streamr.dht.Identifiers;
import streamr.dht.SortedContactList;

using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::dht::contact::SortedContactList;
using streamr::dht::contact::SortedContactListOptions;
namespace contactlistevents = streamr::dht::contact::contactlistevents;

namespace {

constexpr size_t defaultMaxSize = 8;
constexpr size_t largeMaxSize = 10;
constexpr size_t maxSizeThree = 3;
constexpr size_t maxSizeTwo = 2;
constexpr int limitTwo = 2;
constexpr int limitTen = 10;
constexpr int negativeLimit = -2;

struct TestItem {
    DhtAddress nodeId;
    explicit TestItem(const DhtAddressRaw& raw)
        : nodeId(Identifiers::getDhtAddressFromRaw(raw)) {}
    [[nodiscard]] DhtAddress getNodeId() const { return this->nodeId; }
};

static_assert(
    streamr::dht::contact::HasGetNodeId<TestItem>,
    "TestItem provides getNodeId()");
static_assert(
    !streamr::dht::contact::HasGetNodeId<int>,
    "the HasGetNodeId constraint is not vacuous");

DhtAddressRaw rawFromBytes(const std::vector<unsigned char>& bytes) {
    return DhtAddressRaw{std::string(bytes.begin(), bytes.end())};
}

std::shared_ptr<TestItem> createItem(const std::vector<unsigned char>& bytes) {
    return std::make_shared<TestItem>(rawFromBytes(bytes));
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

class SortedContactListTest : public ::testing::Test {
protected:
    std::shared_ptr<TestItem> item0 = createItem({0, 0, 0, 0});
    std::shared_ptr<TestItem> item1 = createItem({0, 0, 0, 1});
    std::shared_ptr<TestItem> item2 = createItem({0, 0, 0, 2});
    std::shared_ptr<TestItem> item3 = createItem({0, 0, 0, 3});
    std::shared_ptr<TestItem> item4 = createItem({0, 0, 0, 4});
};

TEST_F(SortedContactListTest, ComparesIdsCorrectly) {
    SortedContactList<TestItem> list(
        SortedContactListOptions{
            .referenceId = item0->getNodeId(),
            .allowToContainReferenceId = true,
            .maxSize = largeMaxSize});
    EXPECT_EQ(list.compareIds(item0->getNodeId(), item0->getNodeId()), 0);
    EXPECT_EQ(list.compareIds(item1->getNodeId(), item1->getNodeId()), 0);
    EXPECT_EQ(list.compareIds(item0->getNodeId(), item1->getNodeId()), -1);
    EXPECT_EQ(list.compareIds(item0->getNodeId(), item2->getNodeId()), -2);
    EXPECT_EQ(list.compareIds(item1->getNodeId(), item0->getNodeId()), 1);
    EXPECT_EQ(list.compareIds(item2->getNodeId(), item0->getNodeId()), 2);
    EXPECT_EQ(list.compareIds(item2->getNodeId(), item3->getNodeId()), -1);
    EXPECT_EQ(list.compareIds(item1->getNodeId(), item4->getNodeId()), -3);
}

TEST_F(SortedContactListTest, CannotExceedMaxSize) {
    SortedContactList<TestItem> list(
        SortedContactListOptions{
            .referenceId = item0->getNodeId(),
            .allowToContainReferenceId = false,
            .maxSize = maxSizeThree});
    std::vector<std::shared_ptr<TestItem>> removed;
    list.on<contactlistevents::ContactRemoved<TestItem>>(
        [&removed](const std::shared_ptr<TestItem>& contact) {
            removed.push_back(contact);
        });
    list.addContact(item1);
    list.addContact(item4);
    list.addContact(item3);
    list.addContact(item2);
    EXPECT_EQ(list.getSize(), 3U);
    EXPECT_EQ(
        nodeIdsOf(list.getClosestContacts()), nodeIdsOf({item1, item2, item3}));
    EXPECT_EQ(list.getContactIds(), nodeIdsOf({item1, item2, item3}));
    ASSERT_EQ(removed.size(), 1U);
    EXPECT_EQ(removed[0]->getNodeId(), item4->getNodeId());
    EXPECT_FALSE(list.getContact(item4->getNodeId()));
}

TEST_F(SortedContactListTest, RemovingContacts) {
    SortedContactList<TestItem> list(
        SortedContactListOptions{
            .referenceId = item0->getNodeId(),
            .allowToContainReferenceId = false,
            .maxSize = defaultMaxSize});
    std::vector<std::shared_ptr<TestItem>> removed;
    list.on<contactlistevents::ContactRemoved<TestItem>>(
        [&removed](const std::shared_ptr<TestItem>& contact) {
            removed.push_back(contact);
        });
    list.removeContact(Identifiers::createRandomDhtAddress());
    list.addContact(item3);
    list.removeContact(item3->getNodeId());
    list.addContact(item4);
    list.addContact(item3);
    list.addContact(item2);
    list.addContact(item1);
    list.removeContact(item2->getNodeId());
    EXPECT_EQ(list.getSize(), 3U);
    EXPECT_FALSE(list.getContact(item2->getNodeId()));
    auto sorted = list.getContactIds();
    std::ranges::sort(
        sorted, [&list](const DhtAddress& a, const DhtAddress& b) {
            return list.compareIds(a, b) < 0;
        });
    EXPECT_EQ(list.getContactIds(), sorted);
    EXPECT_EQ(
        nodeIdsOf(list.getClosestContacts()), nodeIdsOf({item1, item3, item4}));
    const bool ret = list.removeContact(
        Identifiers::getDhtAddressFromRaw(rawFromBytes({0, 0, 0, 6})));
    EXPECT_FALSE(ret);
    list.removeContact(item3->getNodeId());
    list.removeContact(Identifiers::createRandomDhtAddress());
    EXPECT_EQ(nodeIdsOf(list.getClosestContacts()), nodeIdsOf({item1, item4}));
    ASSERT_EQ(removed.size(), 3U);
    EXPECT_EQ(removed[0]->getNodeId(), item3->getNodeId());
    EXPECT_EQ(removed[1]->getNodeId(), item2->getNodeId());
    EXPECT_EQ(removed[2]->getNodeId(), item3->getNodeId());
}

TEST_F(SortedContactListTest, GetClosestContacts) {
    SortedContactList<TestItem> list(
        SortedContactListOptions{
            .referenceId = item0->getNodeId(),
            .allowToContainReferenceId = false,
            .maxSize = defaultMaxSize});
    list.addContact(item1);
    list.addContact(item3);
    list.addContact(item4);
    list.addContact(item2);
    EXPECT_EQ(
        nodeIdsOf(list.getClosestContacts(limitTwo)),
        nodeIdsOf({item1, item2}));
    EXPECT_EQ(
        nodeIdsOf(list.getClosestContacts(limitTen)),
        nodeIdsOf({item1, item2, item3, item4}));
    EXPECT_EQ(
        nodeIdsOf(list.getClosestContacts()),
        nodeIdsOf({item1, item2, item3, item4}));
    EXPECT_TRUE(list.getClosestContacts(negativeLimit).empty());
}

TEST_F(SortedContactListTest, GetFurthestContacts) {
    SortedContactList<TestItem> list(
        SortedContactListOptions{
            .referenceId = item0->getNodeId(),
            .allowToContainReferenceId = false,
            .maxSize = defaultMaxSize});
    list.addContact(item1);
    list.addContact(item3);
    list.addContact(item4);
    list.addContact(item2);
    EXPECT_EQ(
        nodeIdsOf(list.getFurthestContacts(limitTwo)),
        nodeIdsOf({item4, item3}));
    EXPECT_EQ(
        nodeIdsOf(list.getFurthestContacts(limitTen)),
        nodeIdsOf({item4, item3, item2, item1}));
    EXPECT_EQ(
        nodeIdsOf(list.getFurthestContacts()),
        nodeIdsOf({item4, item3, item2, item1}));
    EXPECT_TRUE(list.getFurthestContacts(negativeLimit).empty());
}

TEST_F(SortedContactListTest, DoesNotEmitContactAddedIfContactDidNotFit) {
    SortedContactList<TestItem> list(
        SortedContactListOptions{
            .referenceId = item0->getNodeId(),
            .allowToContainReferenceId = false,
            .maxSize = maxSizeTwo});
    int added = 0;
    list.on<contactlistevents::ContactAdded<TestItem>>(
        [&added](const std::shared_ptr<TestItem>& /*contact*/) { ++added; });
    list.addContact(item1);
    list.addContact(item2);
    EXPECT_EQ(added, 2);
    list.addContact(item3);
    EXPECT_EQ(added, 2);
    EXPECT_EQ(list.getClosestContacts().size(), 2U);
}
