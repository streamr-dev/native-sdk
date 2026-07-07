// Unit coverage for RingContactList (module streamr.dht.RingContactList).
//
// The TS ring coverage lives in test/benchmark/RingCorrectness.test.ts,
// which drives 900 real DhtNodes joining a ring and compares against
// pre-generated ground-truth data — none of which exists in the C++ port
// yet (DhtNode/joinRing are much later phases). Per the plan (phase A1),
// the ring behaviour is instead covered by this plain unit test exercising
// RingContactList directly: neighbour selection to each side, the
// per-side cap, reference-id / excluded-id filtering, removal and lookup.
//
// A contact's ring id is region(4 bytes) + ipAddress(4 bytes) + the last 7
// bytes of the node id, most-significant first. The region therefore
// dominates the ordering (weight 2^88), so setting distinct regions places
// contacts at controlled, ordered positions on the ring; distinct node ids
// keep their identities (and exclusion) separate.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <gtest/gtest.h>

import streamr.dht.Contact;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.dht.RingContactList;
import streamr.dht.ringIdentifiers;

using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::contact::Contact;
using streamr::dht::contact::getRingIdRawFromPeerDescriptor;
using streamr::dht::contact::RingContactList;
using streamr::dht::contact::RingIdRaw;

namespace {

constexpr size_t kademliaIdLength = 20;
constexpr uint32_t referenceRegion = 100;
constexpr unsigned int byteMask = 0xFF;
constexpr unsigned int byteShift = 8;

PeerDescriptor makePeerDescriptor(uint32_t region) {
    PeerDescriptor peerDescriptor;
    // 20-byte node id, unique per region (encoded in the low bytes, which
    // also form the ring id's low bits — dwarfed by the region).
    std::string nodeId(kademliaIdLength, '\0');
    nodeId[kademliaIdLength - 1] = static_cast<char>(region & byteMask);
    nodeId[kademliaIdLength - 2] =
        static_cast<char>((region >> byteShift) & byteMask);
    peerDescriptor.set_nodeid(nodeId);
    peerDescriptor.set_region(region);
    peerDescriptor.set_type(NodeType::NODEJS);
    return peerDescriptor;
}

static_assert(
    streamr::dht::contact::HasGetPeerDescriptor<Contact>,
    "Contact provides getPeerDescriptor()");
static_assert(
    !streamr::dht::contact::HasGetPeerDescriptor<int>,
    "the HasGetPeerDescriptor constraint is not vacuous");

std::shared_ptr<Contact> makeContact(uint32_t region) {
    return std::make_shared<Contact>(makePeerDescriptor(region));
}

RingIdRaw referenceIdRaw() {
    return getRingIdRawFromPeerDescriptor(makePeerDescriptor(referenceRegion));
}

std::vector<uint32_t> regionsOf(
    const std::vector<std::shared_ptr<Contact>>& contacts) {
    std::vector<uint32_t> regions;
    regions.reserve(contacts.size());
    for (const auto& contact : contacts) {
        regions.push_back(contact->getPeerDescriptor().region());
    }
    return regions;
}

// The distinct regions present, sorted. A contact may legitimately sit on
// BOTH sides when the ring is sparse (an empty side accepts any contact),
// so getAllContacts() can list it twice; the distinct set is what the
// membership assertions care about.
std::vector<uint32_t> sortedUniqueRegions(
    const std::vector<std::shared_ptr<Contact>>& contacts) {
    std::set<uint32_t> regions;
    for (const auto& contact : contacts) {
        regions.insert(contact->getPeerDescriptor().region());
    }
    return {regions.begin(), regions.end()};
}

} // namespace

// The region values below are arbitrary ring positions (test data), so the
// magic-number check is suppressed for the test bodies.
// NOLINTBEGIN(readability-magic-numbers)

TEST(RingContactListTest, KeepsClosestNeighboursPerSide) {
    RingContactList<Contact> list(referenceIdRaw());
    // 10 below and 10 above the reference region (100).
    for (uint32_t region = 90; region < 100; ++region) {
        list.addContact(makeContact(region));
    }
    for (uint32_t region = 101; region <= 110; ++region) {
        list.addContact(makeContact(region));
    }
    const auto closest = list.getClosestContacts();
    // Left = the 5 nearest below, closest first (descending region).
    EXPECT_EQ(
        regionsOf(closest.left), (std::vector<uint32_t>{99, 98, 97, 96, 95}));
    // Right = the 5 nearest above, closest first (ascending region).
    EXPECT_EQ(
        regionsOf(closest.right),
        (std::vector<uint32_t>{101, 102, 103, 104, 105}));
}

TEST(RingContactListTest, LimitPerSide) {
    RingContactList<Contact> list(referenceIdRaw());
    for (uint32_t region = 95; region < 100; ++region) {
        list.addContact(makeContact(region));
    }
    for (uint32_t region = 101; region <= 105; ++region) {
        list.addContact(makeContact(region));
    }
    const auto closest = list.getClosestContacts(2);
    EXPECT_EQ(regionsOf(closest.left), (std::vector<uint32_t>{99, 98}));
    EXPECT_EQ(regionsOf(closest.right), (std::vector<uint32_t>{101, 102}));
}

TEST(RingContactListTest, ExcludesReferenceIdAndExcludedIds) {
    const auto excludedContact = makeContact(101);
    const std::set<streamr::dht::DhtAddress> excludedIds = {
        Identifiers::getNodeIdFromPeerDescriptor(
            excludedContact->getPeerDescriptor())};
    RingContactList<Contact> list(referenceIdRaw(), excludedIds);

    list.addContact(makeContact(referenceRegion)); // same ring id as reference
    list.addContact(excludedContact); // excluded by node id
    list.addContact(makeContact(99));
    list.addContact(makeContact(102));

    // The reference-region and excluded contacts are dropped; only 99 and
    // 102 remain (each may appear on both sides — see sortedUniqueRegions).
    EXPECT_EQ(
        sortedUniqueRegions(list.getAllContacts()),
        (std::vector<uint32_t>{99, 102}));
}

TEST(RingContactListTest, GetContactAndRemoveContact) {
    RingContactList<Contact> list(referenceIdRaw());
    const auto below = makeContact(99);
    const auto above = makeContact(101);
    list.addContact(below);
    list.addContact(above);

    EXPECT_TRUE(list.getContact(below->getPeerDescriptor()));
    EXPECT_TRUE(list.getContact(above->getPeerDescriptor()));
    EXPECT_FALSE(list.getContact(makePeerDescriptor(50)));

    list.removeContact(below);
    EXPECT_FALSE(list.getContact(below->getPeerDescriptor()));
    EXPECT_EQ(regionsOf(list.getAllContacts()), (std::vector<uint32_t>{101}));
}

TEST(RingContactListTest, GetAllContactsReturnsBothSides) {
    RingContactList<Contact> list(referenceIdRaw());
    // Enough on each side that the closest neighbours are distinct per side
    // (no sparse both-sides overlap): 5 below and 5 above.
    for (uint32_t region = 95; region < 100; ++region) {
        list.addContact(makeContact(region));
    }
    for (uint32_t region = 101; region <= 105; ++region) {
        list.addContact(makeContact(region));
    }
    const auto closest = list.getClosestContacts();
    EXPECT_EQ(
        regionsOf(closest.left), (std::vector<uint32_t>{99, 98, 97, 96, 95}));
    EXPECT_EQ(
        regionsOf(closest.right),
        (std::vector<uint32_t>{101, 102, 103, 104, 105}));
    // getAllContacts is left ++ right.
    std::vector<uint32_t> both = regionsOf(closest.left);
    const auto rightRegions = regionsOf(closest.right);
    both.insert(both.end(), rightRegions.begin(), rightRegions.end());
    EXPECT_EQ(regionsOf(list.getAllContacts()), both);
}

// NOLINTEND(readability-magic-numbers)
