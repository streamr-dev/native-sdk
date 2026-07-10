// Ported from packages/dht/test/integration/DhtNodeExternalAPI.test.ts
// (v103.8.0-rc.3): a non-joined node uses another node as its gateway to the
// DHT via the external API RPCs (storeData / externalFetchData /
// externalFindClosestNodes). expectEqualData is adapted: the C++ mock entry
// carries no packed data payload (see TestUtils.createMockDataEntry), so
// entries are identified by their key.
#include <memory>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.RegionPings;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.TestUtils;
import streamr.dht.protos;

#include "DhtNodeTestUtils.hpp" // IWYU pragma: keep — needs the imports above

using ::dht::DataEntry;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::testutils::createMockConnectionDhtNode;
using streamr::dht::testutils::createMockDataEntry;
using streamr::dht::testutils::MockDhtNode;
using streamr::utils::blockingWait;

namespace {

DhtAddress keyOf(const DataEntry& entry) {
    return Identifiers::getDhtAddressFromRaw(
        streamr::dht::DhtAddressRaw{entry.key()});
}

DhtAddress creatorOf(const DataEntry& entry) {
    return Identifiers::getDhtAddressFromRaw(
        streamr::dht::DhtAddressRaw{entry.creator()});
}

// expectEqualData(actual, expected): the TS helper compares the key and the
// unpacked data payload — NOT the creator (storeDataToDht stamps the storing
// node as creator, overriding the mock entry's random one). The C++ mock
// entry has no payload, so the key identifies the entry.
void expectEqualData(const DataEntry& actual, const DataEntry& expected) {
    EXPECT_EQ(keyOf(actual), keyOf(expected));
}

} // namespace

class DhtNodeExternalApiTest : public ::testing::Test {
protected:
    Simulator simulator{LatencyType::NONE};
    std::shared_ptr<MockDhtNode> dhtNode1;
    std::shared_ptr<MockDhtNode> remote;

    void SetUp() override {
        this->dhtNode1 = createMockConnectionDhtNode(this->simulator);
        this->remote = createMockConnectionDhtNode(this->simulator);
        blockingWait(this->dhtNode1->node->joinDht(
            {this->dhtNode1->node->getLocalPeerDescriptor()}));
    }

    void TearDown() override {
        this->dhtNode1->stopNode();
        this->remote->stopNode();
        this->simulator.stop();
    }
};

TEST_F(DhtNodeExternalApiTest, FetchDataHappyPath) {
    const auto entry = createMockDataEntry();
    blockingWait(
        this->dhtNode1->node->storeDataToDht(keyOf(entry), entry.data()));
    const auto foundData =
        blockingWait(this->remote->node->fetchDataFromDhtViaPeer(
            keyOf(entry), this->dhtNode1->node->getLocalPeerDescriptor()));
    ASSERT_FALSE(foundData.empty());
    expectEqualData(foundData[0], entry);
}

TEST_F(DhtNodeExternalApiTest, FetchDataReturnsEmptyArrayIfNoDataFound) {
    const auto foundData =
        blockingWait(this->remote->node->fetchDataFromDhtViaPeer(
            Identifiers::createRandomDhtAddress(),
            this->dhtNode1->node->getLocalPeerDescriptor()));
    EXPECT_TRUE(foundData.empty());
}

TEST_F(DhtNodeExternalApiTest, ExternalStoreDataHappyPath) {
    const auto entry = createMockDataEntry();
    blockingWait(this->remote->node->storeDataToDhtViaPeer(
        keyOf(entry),
        entry.data(),
        this->dhtNode1->node->getLocalPeerDescriptor()));
    const auto foundData =
        blockingWait(this->remote->node->fetchDataFromDhtViaPeer(
            keyOf(entry), this->dhtNode1->node->getLocalPeerDescriptor()));
    ASSERT_FALSE(foundData.empty());
    EXPECT_EQ(keyOf(foundData[0]), keyOf(entry));
    // The creator of externally stored data is the requesting node.
    EXPECT_EQ(
        creatorOf(foundData[0]),
        Identifiers::getNodeIdFromPeerDescriptor(
            this->remote->node->getLocalPeerDescriptor()));
}

TEST_F(DhtNodeExternalApiTest, ExternalFindClosestNodesHappyPath) {
    const auto nodeId = this->remote->node->getNodeId();
    const auto closestNodes =
        blockingWait(this->remote->node->findClosestNodesFromDht(nodeId));
    ASSERT_FALSE(closestNodes.empty());
    EXPECT_EQ(
        Identifiers::getNodeIdFromPeerDescriptor(closestNodes[0]), nodeId);
}
