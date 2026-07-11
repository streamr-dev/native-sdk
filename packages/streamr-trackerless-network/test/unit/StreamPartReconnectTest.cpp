// Ported from packages/trackerless-network/test/unit/
// StreamPartReconnect.test.ts (v103.8.0-rc.3). The fake store manager is
// ported inline from test/utils/fake/FakePeerDescriptorStoreManager.ts
// (its only user); the TS fake force-casts an unrelated class, here it
// overrides the manager's virtual entry points.
// The generated dht protos come ONLY from `import streamr.dht.protos` — a
// textual DhtRpc.pb.h include alongside the BMI makes clangd flag every
// member call on those types as ambiguous.
#include <chrono>
#include <memory>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.trackerlessnetwork.MockDiscoveryLayerNode;
import streamr.trackerlessnetwork.PeerDescriptorStoreManager;
import streamr.trackerlessnetwork.StreamPartReconnect;
import streamr.trackerlessnetwork.TestUtils;
import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::trackerlessnetwork::StreamPartReconnect;
using streamr::trackerlessnetwork::controllayer::PeerDescriptorStoreManager;
using streamr::trackerlessnetwork::controllayer::
    PeerDescriptorStoreManagerOptions;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::trackerlessnetwork::testutils::MockDiscoveryLayerNode;
using streamr::utils::blockingWait;
using streamr::utils::waitForCondition;

namespace {

constexpr auto untilTimeout = std::chrono::seconds(15);
constexpr auto untilPollInterval = std::chrono::milliseconds(50);
constexpr auto testReconnectInterval = std::chrono::milliseconds(1000);

class FakePeerDescriptorStoreManager : public PeerDescriptorStoreManager {
private:
    std::vector<PeerDescriptor> nodes;

public:
    FakePeerDescriptorStoreManager()
        : PeerDescriptorStoreManager(
              PeerDescriptorStoreManagerOptions{
                  .key = Identifiers::createRandomDhtAddress(),
                  .localPeerDescriptor = createMockPeerDescriptor(),
                  .storeInterval = std::nullopt,
                  .fetchDataFromDht = [](auto /*key*/)
                      -> folly::coro::Task<std::vector<DataEntry>> {
                      co_return std::vector<DataEntry>{};
                  },
                  .storeDataToDht = [](auto /*key*/, auto /*data*/)
                      -> folly::coro::Task<std::vector<PeerDescriptor>> {
                      co_return std::vector<PeerDescriptor>{};
                  },
                  .deleteDataFromDht = [](auto /*key*/,
                                          bool /*waitForCompletion*/)
                      -> folly::coro::Task<void> { co_return; }}) {}

    void setNodes(std::vector<PeerDescriptor> newNodes) {
        this->nodes = std::move(newNodes);
    }

    folly::coro::Task<std::vector<PeerDescriptor>> fetchNodes() override {
        co_return this->nodes;
    }

    folly::coro::Task<void> storeAndKeepLocalNode() override { co_return; }

    [[nodiscard]] bool isLocalNodeStored() const override { return true; }
};

} // namespace

class StreamPartReconnectTest : public ::testing::Test {
protected:
    FakePeerDescriptorStoreManager peerDescriptorStoreManager;
    MockDiscoveryLayerNode discoveryLayerNode;
    std::unique_ptr<StreamPartReconnect> streamPartReconnect;

    void SetUp() override {
        this->streamPartReconnect = std::make_unique<StreamPartReconnect>(
            this->discoveryLayerNode, this->peerDescriptorStoreManager);
    }

    void TearDown() override { this->streamPartReconnect->destroy(); }
};

TEST_F(StreamPartReconnectTest, HappyPath) {
    blockingWait(this->streamPartReconnect->reconnect(testReconnectInterval));
    EXPECT_EQ(this->streamPartReconnect->isRunning(), true);
    this->discoveryLayerNode.addNewRandomPeerToKBucket();
    EXPECT_NO_THROW(blockingWait(waitForCondition(
        [this]() { return !this->streamPartReconnect->isRunning(); },
        untilTimeout,
        untilPollInterval)));
}
