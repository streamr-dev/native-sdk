// Ported from packages/trackerless-network/test/unit/NodeList.test.ts
// (v103.8.0-rc.3): add/remove with the size limit, the first/last/random
// accessors with excludes, and the insertion-order contract the
// handshake-interleave protocol depends on.
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

// The TS test drives ordering with literal 3-byte node ids; the byte
// patterns are data, not tunables.
// NOLINTBEGIN(readability-magic-numbers)

import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.TestUtils;
import streamr.dht.Identifiers;
import streamr.dht.protos;

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::trackerlessnetwork::ContentDeliveryRpcRemote;
using streamr::trackerlessnetwork::NodeList;
using streamr::trackerlessnetwork::testutils::
    createMockContentDeliveryRpcRemote;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;

namespace {

constexpr size_t nodeListLimit = 6;

DhtAddress addressFromBytes(std::initializer_list<char> bytes) {
    return Identifiers::getDhtAddressFromRaw(
        DhtAddressRaw{std::string{bytes.begin(), bytes.end()}});
}

PeerDescriptor createDescriptorWithId(const DhtAddress& nodeId) {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(Identifiers::getRawFromDhtAddress(nodeId));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

} // namespace

class NodeListTest : public ::testing::Test {
protected:
    std::vector<DhtAddress> ids = {
        addressFromBytes({1, 1, 1}),
        addressFromBytes({1, 1, 2}),
        addressFromBytes({1, 1, 3}),
        addressFromBytes({1, 1, 4}),
        addressFromBytes({1, 1, 5})};
    DhtAddress ownId = Identifiers::createRandomDhtAddress();
    std::optional<NodeList> nodeList;

    void SetUp() override {
        this->nodeList.emplace(this->ownId, nodeListLimit);
        for (const auto& id : this->ids) {
            this->nodeList->add(
                createMockContentDeliveryRpcRemote(createDescriptorWithId(id)));
        }
    }

    [[nodiscard]] static DhtAddress nodeIdOf(
        const std::shared_ptr<ContentDeliveryRpcRemote>& remote) {
        return Identifiers::getNodeIdFromPeerDescriptor(
            remote->getPeerDescriptor());
    }
};

TEST_F(NodeListTest, Add) {
    const auto newDescriptor = createMockPeerDescriptor();
    this->nodeList->add(createMockContentDeliveryRpcRemote(newDescriptor));
    EXPECT_EQ(
        this->nodeList->has(
            Identifiers::getNodeIdFromPeerDescriptor(newDescriptor)),
        true);

    // The list is at its limit now; further adds are rejected.
    const auto newDescriptor2 = createMockPeerDescriptor();
    this->nodeList->add(createMockContentDeliveryRpcRemote(newDescriptor2));
    EXPECT_EQ(
        this->nodeList->has(
            Identifiers::getNodeIdFromPeerDescriptor(newDescriptor2)),
        false);
}

TEST_F(NodeListTest, Remove) {
    const auto toRemove = this->nodeList->getFirst({});
    const auto nodeId = nodeIdOf(toRemove.value());
    this->nodeList->remove(nodeId);
    EXPECT_EQ(this->nodeList->has(nodeId), false);
}

TEST_F(NodeListTest, GetFirst) {
    const auto closest = this->nodeList->getFirst({});
    EXPECT_EQ(nodeIdOf(closest.value()), addressFromBytes({1, 1, 1}));
}

TEST_F(NodeListTest, GetFirstWithExclude) {
    const auto closest =
        this->nodeList->getFirst({addressFromBytes({1, 1, 1})});
    EXPECT_EQ(nodeIdOf(closest.value()), addressFromBytes({1, 1, 2}));
}

TEST_F(NodeListTest, GetFirstWsOnly) {
    auto descriptor = createMockPeerDescriptor();
    descriptor.mutable_websocket()->set_port(111); // NOLINT
    descriptor.mutable_websocket()->set_host("");
    descriptor.mutable_websocket()->set_tls(false);
    this->nodeList->add(createMockContentDeliveryRpcRemote(descriptor));
    const auto closest = this->nodeList->getFirst({}, true);
    EXPECT_EQ(closest.has_value(), true);
}

TEST_F(NodeListTest, GetLast) {
    const auto last = this->nodeList->getLast({});
    EXPECT_EQ(nodeIdOf(last.value()), addressFromBytes({1, 1, 5}));
}

TEST_F(NodeListTest, GetLastWithExclude) {
    const auto last = this->nodeList->getLast({addressFromBytes({1, 1, 5})});
    EXPECT_EQ(nodeIdOf(last.value()), addressFromBytes({1, 1, 4}));
}

TEST_F(NodeListTest, GetFirstAndLast) {
    const auto results = this->nodeList->getFirstAndLast({});
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results[0], this->nodeList->getFirst({}).value());
    EXPECT_EQ(results[1], this->nodeList->getLast({}).value());
}

TEST_F(NodeListTest, GetFirstEmpty) {
    NodeList emptyList{this->ownId, 2};
    EXPECT_EQ(emptyList.getFirst({}).has_value(), false);
}

TEST_F(NodeListTest, GetLastEmpty) {
    NodeList emptyList{this->ownId, 2};
    EXPECT_EQ(emptyList.getLast({}).has_value(), false);
}

TEST_F(NodeListTest, GetRandomEmpty) {
    NodeList emptyList{this->ownId, 2};
    EXPECT_EQ(emptyList.getRandom({}).has_value(), false);
}

TEST_F(NodeListTest, GetFirstAndLastEmpty) {
    NodeList emptyList{this->ownId, 2};
    EXPECT_EQ(emptyList.getFirstAndLast({}).size(), 0);
}

TEST_F(NodeListTest, GetFirstAndLastWithExclude) {
    const auto results = this->nodeList->getFirstAndLast(
        {addressFromBytes({1, 1, 1}), addressFromBytes({1, 1, 5})});
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(
        results[0],
        this->nodeList->getFirst({addressFromBytes({1, 1, 1})}).value());
    EXPECT_EQ(
        results[1],
        this->nodeList->getLast({addressFromBytes({1, 1, 5})}).value());
}

TEST_F(NodeListTest, ItemsAreInInsertionOrder) {
    NodeList list{Identifiers::createRandomDhtAddress(), 100}; // NOLINT
    const auto item1 = createMockContentDeliveryRpcRemote();
    const auto item2 = createMockContentDeliveryRpcRemote();
    const auto item3 = createMockContentDeliveryRpcRemote();
    const auto item4 = createMockContentDeliveryRpcRemote();
    const auto item5 = createMockContentDeliveryRpcRemote();
    const auto item6 = createMockContentDeliveryRpcRemote();
    list.add(item2);
    list.add(item3);
    list.add(item1);
    list.add(item6);
    list.add(item4);
    list.add(item5);
    EXPECT_EQ(list.getFirst({}).value(), item2);
    EXPECT_EQ(list.getLast({}).value(), item5);
}

// NOLINTEND(readability-magic-numbers)
