// Ported from packages/trackerless-network/test/integration/
// NetworkNode.test.ts (v103.8.0-rc.3): two NetworkNodes over layer-0
// simulator transports — join + broadcast/subscribe and fetchNodeInfo.
//
// NB: TestUtils and the textual pb.h are avoided — this TU composes the
// full NetworkStack + DhtNode + simulator module graph and additional
// BMIs exhaust clang's per-TU source-location space (see the C3/C5 test
// files).
#include <atomic>
#include <memory>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.NetworkNode;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.Simulator;
import streamr.dht.SimulatorTransport;
import streamr.dht.protos;
import streamr.utils.BinaryUtils;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::connection::simulator::LatencyType;
using streamr::dht::connection::simulator::Simulator;
using streamr::dht::connection::simulator::SimulatorTransport;
using streamr::trackerlessnetwork::createNetworkNode;
using streamr::trackerlessnetwork::NetworkNode;
using streamr::trackerlessnetwork::NetworkOptions;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::waitForCondition;

namespace {

// Local copies of the TestUtils factories (see the NB above).
inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(::dht::NodeType::NODEJS);
    return descriptor;
}

StreamMessage createTestMessage(const StreamPartID& streamPartId) {
    StreamMessage msg;
    auto* messageId = msg.mutable_messageid();
    messageId->set_streamid(StreamPartIDUtils::getStreamID(streamPartId));
    messageId->set_streampartition(
        static_cast<int32_t>(
            StreamPartIDUtils::getStreamPartition(streamPartId).value_or(0)));
    messageId->set_timestamp(666); // NOLINT
    messageId->set_sequencenumber(0);
    messageId->set_publisherid(
        BinaryUtils::hexToBinaryString(
            "0x1234567890123456789012345678901234567890"));
    messageId->set_messagechainid("msgChainId");
    auto* previousMessageRef = msg.mutable_previousmessageref();
    previousMessageRef->set_timestamp(665); // NOLINT
    previousMessageRef->set_sequencenumber(0);
    auto* contentMessage = msg.mutable_contentmessage();
    contentMessage->set_content(R"({ "hello": "world" })");
    contentMessage->set_contenttype(ContentType::JSON);
    contentMessage->set_encryptiontype(EncryptionType::NONE);
    msg.set_signaturetype(SignatureType::ECDSA_SECP256K1_EVM);
    msg.set_signature(BinaryUtils::hexToBinaryString("0x1234"));
    return msg;
}

} // namespace

class NetworkNodeIntegrationTest : public ::testing::Test {
protected:
    StreamPartID streamPartId = StreamPartIDUtils::parse("test#0");
    PeerDescriptor pd1 = createMockPeerDescriptor();
    PeerDescriptor pd2 = createMockPeerDescriptor();
    Simulator simulator{LatencyType::NONE};
    std::shared_ptr<SimulatorTransport> transport1;
    std::shared_ptr<SimulatorTransport> transport2;
    std::shared_ptr<NetworkNode> node1;
    std::shared_ptr<NetworkNode> node2;

    void SetUp() override {
        this->transport1 =
            std::make_shared<SimulatorTransport>(this->pd1, this->simulator);
        this->transport1->start();
        this->transport2 =
            std::make_shared<SimulatorTransport>(this->pd2, this->simulator);
        this->transport2->start();

        this->node1 = createNetworkNode(
            NetworkOptions{
                .layer0 = DhtNodeOptions{
                    .transport = this->transport1.get(),
                    .connectionsView = this->transport1.get(),
                    .peerDescriptor = this->pd1,
                    .entryPoints = {this->pd1}}});
        this->node2 = createNetworkNode(
            NetworkOptions{
                .layer0 = DhtNodeOptions{
                    .transport = this->transport2.get(),
                    .connectionsView = this->transport2.get(),
                    .peerDescriptor = this->pd2,
                    .entryPoints = {this->pd1}}});

        blockingWait(this->node1->start());
        this->node1->setStreamPartEntryPoints(this->streamPartId, {this->pd1});
        blockingWait(this->node2->start());
        this->node2->setStreamPartEntryPoints(this->streamPartId, {this->pd1});
    }

    void TearDown() override {
        if (this->node1) {
            blockingWait(this->node1->stop());
        }
        if (this->node2) {
            blockingWait(this->node2->stop());
        }
        this->transport1->stop();
        this->transport2->stop();
        this->simulator.stop();
    }
};

TEST_F(NetworkNodeIntegrationTest, WaitForJoinPlusBroadcastAndSubscribe) {
    std::atomic<int> msgCount{0};
    blockingWait(this->node1->join(this->streamPartId));
    this->node1->addMessageListener([&msgCount](const StreamMessage& msg) {
        EXPECT_EQ(msg.messageid().timestamp(), 666);
        EXPECT_EQ(msg.messageid().sequencenumber(), 0);
        msgCount += 1;
    });
    blockingWait(this->node2->broadcast(createTestMessage(this->streamPartId)));
    blockingWait(
        waitForCondition([&msgCount]() { return msgCount.load() == 1; }));
}

TEST_F(NetworkNodeIntegrationTest, FetchNodeInfo) {
    blockingWait(this->node1->join(this->streamPartId));
    blockingWait(this->node2->join(this->streamPartId));
    const auto result1 = blockingWait(this->node1->fetchNodeInfo(this->pd2));
    const auto result2 = blockingWait(this->node2->fetchNodeInfo(this->pd1));
    const auto result3 = blockingWait(
        this->node1->fetchNodeInfo(this->node1->getPeerDescriptor()));
    EXPECT_EQ(result1.streampartitions_size(), 1);
    EXPECT_EQ(result2.streampartitions_size(), 1);
    EXPECT_EQ(result3.streampartitions_size(), 1);
    EXPECT_EQ(result1.controllayer().connections_size(), 1);
    EXPECT_EQ(result2.controllayer().connections_size(), 1);
    EXPECT_EQ(result3.controllayer().connections_size(), 1);
    EXPECT_EQ(result1.controllayer().neighbors_size(), 1);
    EXPECT_EQ(result2.controllayer().neighbors_size(), 1);
    EXPECT_EQ(result3.controllayer().neighbors_size(), 1);
}
