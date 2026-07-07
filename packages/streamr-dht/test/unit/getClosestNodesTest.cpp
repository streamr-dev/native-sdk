// Ported from packages/dht/test/unit/getClosestNodes.test.ts
// (v103.8.0-rc.3). The TS test excludes a random sample of 2 node ids;
// here it excludes the first two, which exercises the same filtering.
#include <algorithm>
#include <cstddef>
#include <set>
#include <vector>
#include <gtest/gtest.h>

import streamr.dht.getClosestNodes;
import streamr.dht.getPeerDistance;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.dht.TestUtils;

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::dht::contact::getClosestNodes;
using streamr::dht::contact::GetClosestNodesOptions;
using streamr::dht::helpers::getPeerDistance;
using streamr::dht::testutils::createMockPeerDescriptor;

namespace {

constexpr size_t descriptorCount = 10;
constexpr size_t maxCount = 5;

std::vector<DhtAddress> nodeIdsOf(
    const std::vector<PeerDescriptor>& peerDescriptors) {
    std::vector<DhtAddress> ids;
    ids.reserve(peerDescriptors.size());
    for (const auto& peerDescriptor : peerDescriptors) {
        ids.push_back(Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor));
    }
    return ids;
}

} // namespace

TEST(getClosestNodes, HappyPath) {
    std::vector<PeerDescriptor> peerDescriptors;
    peerDescriptors.reserve(descriptorCount);
    for (size_t i = 0; i < descriptorCount; ++i) {
        peerDescriptors.push_back(createMockPeerDescriptor());
    }
    const DhtAddress referenceId = Identifiers::createRandomDhtAddress();
    const std::set<DhtAddress> excluded = {
        Identifiers::getNodeIdFromPeerDescriptor(peerDescriptors[0]),
        Identifiers::getNodeIdFromPeerDescriptor(peerDescriptors[1])};

    const std::vector<PeerDescriptor> actual = getClosestNodes(
        referenceId,
        peerDescriptors,
        GetClosestNodesOptions{
            .maxCount = maxCount, .excludedNodeIds = excluded});

    const DhtAddressRaw referenceIdRaw =
        Identifiers::getRawFromDhtAddress(referenceId);
    std::vector<PeerDescriptor> expected;
    for (const auto& peerDescriptor : peerDescriptors) {
        if (!excluded.contains(
                Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor))) {
            expected.push_back(peerDescriptor);
        }
    }
    std::ranges::stable_sort(
        expected,
        [&referenceIdRaw](const PeerDescriptor& a, const PeerDescriptor& b) {
            return getPeerDistance(DhtAddressRaw{a.nodeid()}, referenceIdRaw) <
                getPeerDistance(DhtAddressRaw{b.nodeid()}, referenceIdRaw);
        });
    if (expected.size() > maxCount) {
        expected.resize(maxCount);
    }

    EXPECT_EQ(nodeIdsOf(actual), nodeIdsOf(expected));
}
