// Ported from packages/trackerless-network/test/unit/Inspector.test.ts
// (v103.8.0-rc.3). The TS setTimeout(..., 250) that marks the messages
// mid-inspection becomes a helper thread; openInspectConnection is
// overridden with a counting fake like the jest mock, the close path runs
// the real temporary-connection notify over a FakeTransport.
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.dht.FakeTransport;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;
import streamr.trackerlessnetwork.Inspector;
import streamr.trackerlessnetwork.TestUtils;
import streamr.trackerlessnetwork.protos;
import streamr.utils.CoroutineHelper;
import streamr.utils.StreamPartID;

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::connection::LockID;
using streamr::dht::transport::FakeTransport;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::trackerlessnetwork::inspection::Inspector;
using streamr::trackerlessnetwork::inspection::InspectorOptions;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::trackerlessnetwork::testutils::MockConnectionLocker;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartIDUtils;

namespace {

constexpr auto markDelay = std::chrono::milliseconds(250);
constexpr int64_t testTimestamp = 12345;

::MessageID createMessageRef() {
    ::MessageID messageId;
    messageId.set_streamid("stream");
    messageId.set_messagechainid("messageChain0");
    messageId.set_streampartition(0);
    messageId.set_sequencenumber(0);
    messageId.set_timestamp(testTimestamp);
    messageId.set_publisherid("publisher");
    return messageId;
}

} // namespace

class InspectorTest : public ::testing::Test {
protected:
    PeerDescriptor inspectorDescriptor = createMockPeerDescriptor();
    PeerDescriptor inspectedDescriptor = createMockPeerDescriptor();
    DhtAddress nodeId = Identifiers::createRandomDhtAddress();
    ::MessageID messageRef = createMessageRef();
    std::atomic<int> mockConnectCalls = 0;
    FakeTransport transport{
        this->inspectorDescriptor, [](const auto& /*message*/) {}};
    ListeningRpcCommunicator rpcCommunicator{
        ServiceID{"inspector"}, this->transport};
    MockConnectionLocker connectionLocker;
    std::unique_ptr<Inspector> inspector;

    void SetUp() override {
        this->inspector = std::make_unique<Inspector>(InspectorOptions{
            .localPeerDescriptor = this->inspectorDescriptor,
            .streamPartId = StreamPartIDUtils::parse("stream#0"),
            .rpcCommunicator = this->rpcCommunicator,
            .connectionLocker = this->connectionLocker,
            .inspectionTimeout = std::nullopt,
            .openInspectConnection =
                [this](PeerDescriptor /*peerDescriptor*/, LockID /*lockId*/)
                -> folly::coro::Task<void> {
                this->mockConnectCalls++;
                co_return;
            },
            .closeInspectConnection = {}});
    }

    void TearDown() override { this->inspector->stop(); }
};

TEST_F(InspectorTest, OpensInspectionConnectionAndRunsSuccessfully) {
    std::thread marker([this]() {
        std::this_thread::sleep_for(markDelay);
        this->inspector->markMessage(
            Identifiers::getNodeIdFromPeerDescriptor(this->inspectedDescriptor),
            this->messageRef);
        this->inspector->markMessage(this->nodeId, this->messageRef);
    });
    const bool success =
        blockingWait(this->inspector->inspect(this->inspectedDescriptor));
    marker.join();
    EXPECT_EQ(success, true);
    EXPECT_EQ(
        this->inspector->isInspected(
            Identifiers::getNodeIdFromPeerDescriptor(
                this->inspectedDescriptor)),
        false);
    EXPECT_EQ(this->mockConnectCalls, 1);
}
