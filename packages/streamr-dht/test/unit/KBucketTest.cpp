// Unit coverage for KBucket (module streamr.dht.KBucket), derived from the
// behaviour contract of the npm k-bucket library's own test suite
// (add / split / ping / closest / remove / update / count / toArray),
// exercised through the public API (the internal tree is private).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <gtest/gtest.h>

import streamr.dht.getPeerDistance;
import streamr.dht.Identifiers;
import streamr.dht.KBucket;

using streamr::dht::DhtAddressRaw;
using streamr::dht::contact::KBucket;
using streamr::dht::contact::KBucketOptions;
using streamr::dht::helpers::getPeerDistance;
namespace kbucketevents = streamr::dht::contact::kbucketevents;

namespace {

constexpr size_t defaultBucketSize = 20;
constexpr size_t defaultNodesToPing = 3;

struct MockContact {
    DhtAddressRaw id;
    int64_t vectorClock;
    MockContact(DhtAddressRaw id, int64_t vectorClock)
        : id(std::move(id)), vectorClock(vectorClock) {}
    [[nodiscard]] DhtAddressRaw getId() const { return this->id; }
    [[nodiscard]] int64_t getVectorClock() const { return this->vectorClock; }
};

static_assert(
    streamr::dht::contact::KBucketContact<MockContact>,
    "MockContact provides getId()/getVectorClock()");
static_assert(
    !streamr::dht::contact::KBucketContact<int>,
    "the KBucketContact constraint is not vacuous");

DhtAddressRaw rawFromBytes(const std::vector<unsigned char>& bytes) {
    return DhtAddressRaw{std::string(bytes.begin(), bytes.end())};
}

std::shared_ptr<MockContact> makeContact(
    const std::vector<unsigned char>& bytes, int64_t vectorClock = 0) {
    return std::make_shared<MockContact>(rawFromBytes(bytes), vectorClock);
}

KBucket<MockContact> makeBucket(const std::vector<unsigned char>& localId) {
    return KBucket<MockContact>(
        KBucketOptions{.localNodeId = rawFromBytes(localId)});
}

} // namespace

// NOLINTBEGIN(readability-magic-numbers) — byte-array ids and vector clocks
// are arbitrary test data.

TEST(KBucketTest, AddingContactPlacesItInBucket) {
    auto bucket = makeBucket({0x00});
    const auto contact = makeContact({'a'});
    bucket.add(contact);
    EXPECT_EQ(bucket.count(), 1U);
    EXPECT_EQ(bucket.get(rawFromBytes({'a'})), contact);
    ASSERT_EQ(bucket.toArray().size(), 1U);
    EXPECT_EQ(bucket.toArray()[0], contact);
}

TEST(KBucketTest, AddingExistingContactDoesNotIncreaseCount) {
    auto bucket = makeBucket({0x00});
    bucket.add(makeContact({'a'}));
    bucket.add(makeContact({'a'}));
    EXPECT_EQ(bucket.count(), 1U);
}

TEST(KBucketTest, AddingSameContactMovesItToTheEnd) {
    auto bucket = makeBucket({0x00});
    const auto contactA = makeContact({'a'});
    const auto contactB = makeContact({'b'});
    bucket.add(contactA);
    bucket.add(contactB);
    EXPECT_EQ(
        bucket.toArray(),
        (std::vector<std::shared_ptr<MockContact>>{contactA, contactB}));

    bool updated = false;
    bucket.on<kbucketevents::Updated<MockContact>>(
        [&updated](
            const std::shared_ptr<MockContact>& /*incumbent*/,
            const std::shared_ptr<MockContact>& /*selection*/) {
            updated = true;
        });
    const auto contactANewer = makeContact({'a'});
    bucket.add(contactANewer);
    EXPECT_EQ(
        bucket.toArray(),
        (std::vector<std::shared_ptr<MockContact>>{contactB, contactANewer}));
    EXPECT_TRUE(updated);
}

TEST(KBucketTest, AddingNewContactEmitsAdded) {
    auto bucket = makeBucket({0x00});
    std::shared_ptr<MockContact> added;
    bucket.on<kbucketevents::Added<MockContact>>(
        [&added](const std::shared_ptr<MockContact>& contact) {
            added = contact;
        });
    const auto contact = makeContact({'a'});
    bucket.add(contact);
    EXPECT_EQ(added, contact);
}

TEST(KBucketTest, SplittingRetainsAllContactsBeyondBucketSize) {
    // With localNodeId near the added ids, a full bucket splits instead of
    // pinging, so all numberOfNodesPerKBucket + 1 contacts are kept.
    auto bucket = makeBucket({0x00});
    for (unsigned char i = 0; i <= defaultBucketSize; ++i) {
        bucket.add(makeContact({i}));
    }
    EXPECT_EQ(bucket.count(), defaultBucketSize + 1);
    for (unsigned char i = 0; i <= defaultBucketSize; ++i) {
        EXPECT_TRUE(bucket.get(rawFromBytes({i}))) << "missing contact " << i;
    }
}

TEST(KBucketTest, NonSplittableFullBucketEmitsPingAndRejectsContact) {
    // All ids are in the half "far" from localNodeId (high bit set), so the
    // split marks their bucket dontSplit; the next contact overflows it and
    // triggers a ping of the longest-uncontacted nodes instead of an add.
    auto bucket = makeBucket({0x00});
    for (unsigned char i = 0; i < defaultBucketSize; ++i) {
        bucket.add(makeContact({static_cast<unsigned char>(0x80 + i)}));
    }
    std::vector<std::shared_ptr<MockContact>> pinged;
    std::shared_ptr<MockContact> replacement;
    bucket.on<kbucketevents::Ping<MockContact>>(
        [&pinged, &replacement](
            const std::vector<std::shared_ptr<MockContact>>& contacts,
            const std::shared_ptr<MockContact>& candidate) {
            pinged = contacts;
            replacement = candidate;
        });
    const auto extra =
        makeContact({static_cast<unsigned char>(0x80 + defaultBucketSize)});
    bucket.add(extra);

    EXPECT_EQ(pinged.size(), defaultNodesToPing);
    EXPECT_EQ(replacement, extra);
    // the least-recently-contacted contacts are offered for pinging
    EXPECT_EQ(pinged[0]->getId(), rawFromBytes({0x80}));
    EXPECT_EQ(pinged[1]->getId(), rawFromBytes({0x81}));
    EXPECT_EQ(pinged[2]->getId(), rawFromBytes({0x82}));
    EXPECT_EQ(bucket.count(), defaultBucketSize);
    EXPECT_FALSE(bucket.get(extra->getId()));
}

TEST(KBucketTest, ClosestReturnsNClosestByXorDistance) {
    auto bucket = makeBucket({0x00});
    std::vector<std::shared_ptr<MockContact>> contacts = {
        makeContact({0x00, 0x01}),
        makeContact({0x00, 0x05}),
        makeContact({0x10, 0x00}),
        makeContact({0x20, 0x03}),
        makeContact({0x40, 0x0a}),
        makeContact({0x80, 0x00})};
    for (const auto& contact : contacts) {
        bucket.add(contact);
    }
    const DhtAddressRaw target = rawFromBytes({0x00, 0x02});
    const auto actual = bucket.closest(target, defaultNodesToPing);

    auto expected = contacts;
    std::ranges::stable_sort(
        expected,
        [&target](
            const std::shared_ptr<MockContact>& a,
            const std::shared_ptr<MockContact>& b) {
            return getPeerDistance(a->getId(), target) <
                getPeerDistance(b->getId(), target);
        });
    expected.resize(defaultNodesToPing);
    EXPECT_EQ(actual, expected);
}

TEST(KBucketTest, ClosestWithoutLimitReturnsAllSortedByDistance) {
    auto bucket = makeBucket({0x00});
    const auto near = makeContact({0x00, 0x01});
    const auto mid = makeContact({0x10, 0x00});
    const auto far = makeContact({0x80, 0x00});
    bucket.add(far);
    bucket.add(near);
    bucket.add(mid);
    const auto actual = bucket.closest(rawFromBytes({0x00, 0x00}));
    EXPECT_EQ(
        actual, (std::vector<std::shared_ptr<MockContact>>{near, mid, far}));
}

TEST(KBucketTest, RemoveEmitsRemovedAndDropsContact) {
    auto bucket = makeBucket({0x00});
    const auto contact = makeContact({'a'});
    bucket.add(contact);
    std::shared_ptr<MockContact> removed;
    bucket.on<kbucketevents::Removed<MockContact>>(
        [&removed](const std::shared_ptr<MockContact>& c) { removed = c; });
    bucket.remove(rawFromBytes({'a'}));
    EXPECT_EQ(removed, contact);
    EXPECT_FALSE(bucket.get(rawFromBytes({'a'})));
    EXPECT_EQ(bucket.count(), 0U);
}

TEST(KBucketTest, RemovingUnknownIdIsANoOp) {
    auto bucket = makeBucket({0x00});
    bucket.add(makeContact({'a'}));
    bool removed = false;
    bucket.on<kbucketevents::Removed<MockContact>>(
        [&removed](const std::shared_ptr<MockContact>& /*c*/) {
            removed = true;
        });
    bucket.remove(rawFromBytes({'b'}));
    EXPECT_FALSE(removed);
    EXPECT_EQ(bucket.count(), 1U);
}

TEST(KBucketTest, LowerVectorClockIsDropped) {
    auto bucket = makeBucket({0x00});
    bucket.add(makeContact({'a'}, 3));
    bool updated = false;
    bucket.on<kbucketevents::Updated<MockContact>>(
        [&updated](
            const std::shared_ptr<MockContact>& /*incumbent*/,
            const std::shared_ptr<MockContact>& /*selection*/) {
            updated = true;
        });
    bucket.add(makeContact({'a'}, 2));
    EXPECT_EQ(bucket.get(rawFromBytes({'a'}))->getVectorClock(), 3);
    EXPECT_FALSE(updated);
}

TEST(KBucketTest, HigherVectorClockWinsAndEmitsUpdated) {
    auto bucket = makeBucket({0x00});
    bucket.add(makeContact({'a'}, 3));
    std::shared_ptr<MockContact> updatedIncumbent;
    std::shared_ptr<MockContact> updatedSelection;
    bucket.on<kbucketevents::Updated<MockContact>>(
        [&updatedIncumbent, &updatedSelection](
            const std::shared_ptr<MockContact>& incumbent,
            const std::shared_ptr<MockContact>& selection) {
            updatedIncumbent = incumbent;
            updatedSelection = selection;
        });
    const auto newer = makeContact({'a'}, 4);
    bucket.add(newer);
    EXPECT_EQ(bucket.get(rawFromBytes({'a'}))->getVectorClock(), 4);
    EXPECT_EQ(updatedSelection, newer);
    EXPECT_EQ(updatedIncumbent->getVectorClock(), 3);
}

TEST(KBucketTest, CountAndToArrayReflectContents) {
    auto bucket = makeBucket({0x00});
    EXPECT_EQ(bucket.count(), 0U);
    EXPECT_TRUE(bucket.toArray().empty());
    bucket.add(makeContact({'a'}));
    bucket.add(makeContact({'b'}));
    bucket.add(makeContact({'c'}));
    EXPECT_EQ(bucket.count(), 3U);
    EXPECT_EQ(bucket.toArray().size(), 3U);
}

// NOLINTEND(readability-magic-numbers)
