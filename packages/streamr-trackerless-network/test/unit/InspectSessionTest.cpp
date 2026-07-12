// Ported from packages/trackerless-network/test/unit/
// InspectSession.test.ts (v103.8.0-rc.3). The TS Promise.all([waitForEvent,
// markMessage]) pairs become folly collectAll: the waitForEvent coroutine
// registers its listener before suspending, then the mark task fires the
// event.
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.dht.Identifiers;
import streamr.trackerlessnetwork.InspectSession;
import streamr.trackerlessnetwork.protos;
import streamr.utils.CoroutineHelper;
import streamr.utils.waitForEvent;

using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::trackerlessnetwork::inspection::InspectSession;
using streamr::trackerlessnetwork::inspection::InspectSessionOptions;
using streamr::trackerlessnetwork::inspection::inspectsessionevents::Done;
using streamr::utils::blockingWait;
using streamr::utils::waitForEvent;

namespace {

constexpr auto eventTimeout = std::chrono::milliseconds(100);
constexpr int64_t testTimestamp = 12345;

::MessageID createMessageId(const std::string& messageChainId) {
    ::MessageID messageId;
    messageId.set_streamid("stream");
    messageId.set_messagechainid(messageChainId);
    messageId.set_streampartition(0);
    messageId.set_sequencenumber(0);
    messageId.set_timestamp(testTimestamp);
    messageId.set_publisherid("publisherId");
    return messageId;
}

} // namespace

class InspectSessionTest : public ::testing::Test {
protected:
    ::MessageID messageId1 = createMessageId("messageChain0");
    ::MessageID messageId2 = createMessageId("messageChain1");
    // Fresh per test like the TS beforeEach (gtest constructs a new
    // fixture per test; DhtAddress has no default constructor).
    DhtAddress inspectedNode = Identifiers::createRandomDhtAddress();
    DhtAddress anotherNode = Identifiers::createRandomDhtAddress();
    std::unique_ptr<InspectSession> inspectSession;

    void SetUp() override {
        this->inspectSession = std::make_unique<InspectSession>(
            InspectSessionOptions{.inspectedNode = this->inspectedNode});
    }

    void TearDown() override { this->inspectSession->stop(); }

    // TS Promise.all([waitForEvent(session, 'done', 100), markMessage(...)]).
    void expectDoneWhenMarking(
        const DhtAddress& remoteNodeId, const ::MessageID& messageId) {
        blockingWait(
            folly::coro::collectAll(
                waitForEvent<Done>(this->inspectSession.get(), eventTimeout),
                folly::coro::co_invoke(
                    [this, &remoteNodeId, &messageId]()
                        -> folly::coro::Task<void> {
                        this->inspectSession->markMessage(
                            remoteNodeId, messageId);
                        co_return;
                    })));
    }
};

TEST_F(InspectSessionTest, ShouldMarkMessage) {
    this->inspectSession->markMessage(this->inspectedNode, this->messageId1);
    EXPECT_EQ(this->inspectSession->getInspectedMessageCount(), 1);
    this->inspectSession->markMessage(this->inspectedNode, this->messageId2);
    EXPECT_EQ(this->inspectSession->getInspectedMessageCount(), 2);
}

TEST_F(InspectSessionTest, ShouldEmitDoneWhenInspectedNodeSendsSeenMessage) {
    this->inspectSession->markMessage(this->anotherNode, this->messageId1);
    this->expectDoneWhenMarking(this->inspectedNode, this->messageId1);
    EXPECT_EQ(this->inspectSession->getInspectedMessageCount(), 1);
}

TEST_F(
    InspectSessionTest,
    ShouldEmitDoneWhenAnotherNodeSendsMessageAfterInspectedNode) {
    this->inspectSession->markMessage(this->inspectedNode, this->messageId1);
    this->expectDoneWhenMarking(this->anotherNode, this->messageId1);
    EXPECT_EQ(this->inspectSession->getInspectedMessageCount(), 1);
}

TEST_F(InspectSessionTest, ShouldNotEmitDoneIfMessageIdsDoNotMatch) {
    this->inspectSession->markMessage(this->inspectedNode, this->messageId1);
    EXPECT_THROW(
        this->expectDoneWhenMarking(this->anotherNode, this->messageId2),
        std::exception);
    EXPECT_EQ(this->inspectSession->getInspectedMessageCount(), 2);
}
