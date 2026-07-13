// Ported from packages/trackerless-network/test/integration/
// NodeInfoRpc.test.ts (v103.8.0-rc.3): a NodeInfoClient on a third
// transport queries a NetworkStack that shares two stream parts with
// another stack.
//
// NB: TestUtils and the textual pb.h are avoided — this TU composes the
// full NetworkStack + DhtNode + simulator module graph and additional
// BMIs exhaust clang's per-TU source-location space (see the C3/C5 test
// files).
#include <algorithm>
#include <memory>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.trackerlessnetwork.NodeInfoClient;
import streamr.trackerlessnetwork.NodeInfoRpcLocal;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.protos;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::connection::simulator::SimulatorTransport;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::trackerlessnetwork::NetworkOptions;
using streamr::trackerlessnetwork::NetworkStack;
using streamr::trackerlessnetwork::NodeInfoClient;
using streamr::trackerlessnetwork::nodeInfoRpcServiceId;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::waitForCondition;

namespace {

inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

bool containsPeer(const auto& repeatedField, const PeerDescriptor& expected) {
    return std::ranges::any_of(
        repeatedField, [&expected](const PeerDescriptor& descriptor) {
            return descriptor.nodeid() == expected.nodeid();
        });
}

} // namespace

class NodeInfoRpcTest : public ::testing::Test {
protected:
    PeerDescriptor requesteePeerDescriptor = createMockPeerDescriptor();
    PeerDescriptor otherPeerDescriptor = createMockPeerDescriptor();
    PeerDescriptor requestorPeerDescriptor = createMockPeerDescriptor();
    Simulator simulator{LatencyType::NONE};
    std::shared_ptr<SimulatorTransport> requesteeTransport;
    std::shared_ptr<SimulatorTransport> otherTransport;
    std::shared_ptr<SimulatorTransport> requestorTransport;
    std::shared_ptr<NetworkStack> requesteeStack;
    std::shared_ptr<NetworkStack> otherStack;
    std::unique_ptr<ListeningRpcCommunicator> requestorCommunicator;
    std::unique_ptr<NodeInfoClient> nodeInfoClient;

    void SetUp() override {
        this->requesteeTransport = std::make_shared<SimulatorTransport>(
            this->requesteePeerDescriptor, this->simulator);
        this->otherTransport = std::make_shared<SimulatorTransport>(
            this->otherPeerDescriptor, this->simulator);
        this->requestorTransport = std::make_shared<SimulatorTransport>(
            this->requestorPeerDescriptor, this->simulator);
        this->requesteeTransport->start();
        this->otherTransport->start();
        this->requestorTransport->start();
        this->requesteeStack = std::make_shared<NetworkStack>(NetworkOptions{
            .layer0 = DhtNodeOptions{
                .transport = this->requesteeTransport.get(),
                .connectionsView = this->requesteeTransport.get(),
                .peerDescriptor = this->requesteePeerDescriptor,
                .entryPoints = {this->requesteePeerDescriptor}}});
        this->otherStack = std::make_shared<NetworkStack>(NetworkOptions{
            .layer0 = DhtNodeOptions{
                .transport = this->otherTransport.get(),
                .connectionsView = this->otherTransport.get(),
                .peerDescriptor = this->otherPeerDescriptor,
                .entryPoints = {this->requesteePeerDescriptor}}});
        blockingWait(this->requesteeStack->start());
        blockingWait(this->otherStack->start());
        this->requestorCommunicator =
            std::make_unique<ListeningRpcCommunicator>(
                ServiceID{nodeInfoRpcServiceId}, *this->requestorTransport);
        this->nodeInfoClient = std::make_unique<NodeInfoClient>(
            this->requestorPeerDescriptor, *this->requestorCommunicator);
    }

    void TearDown() override {
        if (this->requesteeStack) {
            blockingWait(this->requesteeStack->stop());
        }
        if (this->otherStack) {
            blockingWait(this->otherStack->stop());
        }
        if (this->requestorCommunicator) {
            this->requestorCommunicator->destroy();
        }
        this->requesteeTransport->stop();
        this->otherTransport->stop();
        this->requestorTransport->stop();
        this->simulator.stop();
    }
};

TEST_F(NodeInfoRpcTest, HappyPath) {
    const auto streamPartId1 = StreamPartIDUtils::parse("stream1#0");
    const auto streamPartId2 = StreamPartIDUtils::parse("stream2#0");
    this->requesteeStack->getContentDeliveryManager().joinStreamPart(
        streamPartId1);
    this->otherStack->getContentDeliveryManager().joinStreamPart(streamPartId1);
    this->requesteeStack->getContentDeliveryManager().joinStreamPart(
        streamPartId2);
    this->otherStack->getContentDeliveryManager().joinStreamPart(streamPartId2);
    blockingWait(waitForCondition(
        [this, &streamPartId1, &streamPartId2]() {
            return this->requesteeStack->getContentDeliveryManager()
                       .getNeighbors(streamPartId1)
                       .size() == 1 &&
                this->otherStack->getContentDeliveryManager()
                    .getNeighbors(streamPartId1)
                    .size() == 1 &&
                this->requesteeStack->getContentDeliveryManager()
                    .getNeighbors(streamPartId2)
                    .size() == 1 &&
                this->otherStack->getContentDeliveryManager()
                    .getNeighbors(streamPartId2)
                    .size() == 1;
        },
        std::chrono::seconds(15))); // NOLINT

    const auto result = blockingWait(
        this->nodeInfoClient->getInfo(this->requesteePeerDescriptor));

    EXPECT_EQ(
        result.peerdescriptor().nodeid(),
        this->requesteePeerDescriptor.nodeid());
    ASSERT_EQ(result.controllayer().neighbors_size(), 1);
    EXPECT_TRUE(containsPeer(
        result.controllayer().neighbors(), this->otherPeerDescriptor));
    EXPECT_EQ(result.controllayer().connections_size(), 2);
    EXPECT_TRUE(containsPeer(
        result.controllayer().connections(), this->otherPeerDescriptor));
    EXPECT_TRUE(containsPeer(
        result.controllayer().connections(), this->requestorPeerDescriptor));
    ASSERT_EQ(result.streampartitions_size(), 2);
    for (const auto& partition : result.streampartitions()) {
        EXPECT_TRUE(
            partition.id() == streamPartId1 || partition.id() == streamPartId2);
        ASSERT_EQ(partition.controllayerneighbors_size(), 1);
        EXPECT_TRUE(containsPeer(
            partition.controllayerneighbors(), this->otherPeerDescriptor));
        ASSERT_EQ(partition.contentdeliverylayerneighbors_size(), 1);
        EXPECT_EQ(
            partition.contentdeliverylayerneighbors(0)
                .peerdescriptor()
                .nodeid(),
            this->otherPeerDescriptor.nodeid());
    }
    EXPECT_FALSE(result.applicationversion().empty());
}
