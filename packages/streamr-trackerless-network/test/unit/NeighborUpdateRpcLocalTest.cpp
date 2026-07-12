// Ported from packages/trackerless-network/test/unit/
// NeighborUpdateRpcLocal.test.ts (v103.8.0-rc.3): the neighbor-update
// server learns contacts from the caller and asks to be removed only in
// the right situations.
//
// NB: NetworkRpc types are consumed ONLY through the
// streamr.trackerlessnetwork.protos module (no textual NetworkRpc.pb.h
// include) — see TestUtilsTest.cpp for the clangd rationale.
#include <memory>
#include <optional>
#include <set>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

import streamr.trackerlessnetwork.ContentDeliveryRpcRemote;
import streamr.trackerlessnetwork.NeighborFinder;
import streamr.trackerlessnetwork.NeighborUpdateRpcLocal;
import streamr.trackerlessnetwork.NetworkRpcClient;
import streamr.trackerlessnetwork.NodeList;
import streamr.trackerlessnetwork.TestUtils;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtCallContext;
import streamr.dht.FakeTransport;
import streamr.dht.Identifiers;
import streamr.dht.ListeningRpcCommunicator;
import streamr.dht.protos;
import streamr.utils.StreamPartID;

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::ServiceID;
using streamr::dht::rpcprotocol::DhtCallContext;
using streamr::dht::transport::FakeTransport;
using streamr::dht::transport::ListeningRpcCommunicator;
using streamr::trackerlessnetwork::ContentDeliveryRpcClient;
using streamr::trackerlessnetwork::ContentDeliveryRpcRemote;
using streamr::trackerlessnetwork::NodeList;
using streamr::trackerlessnetwork::neighbordiscovery::INeighborFinder;
using streamr::trackerlessnetwork::neighbordiscovery::NeighborUpdateRpcLocal;
using streamr::trackerlessnetwork::neighbordiscovery::
    NeighborUpdateRpcLocalOptions;
using streamr::trackerlessnetwork::testutils::createMockPeerDescriptor;
using streamr::utils::StreamPartIDUtils;

namespace {

constexpr size_t neighborTargetCount = 4;

// The TS test injects `{ start: jest.fn() }`.
class MockNeighborFinder : public INeighborFinder {
public:
    size_t startCalls = 0;

    using INeighborFinder::start;
    void start(std::vector<DhtAddress> /*excluded*/) override {
        this->startCalls++;
    }
    void stop() override {}
    [[nodiscard]] bool isRunning() const override { return false; }
};

} // namespace

class NeighborUpdateRpcLocalTest : public ::testing::Test {
protected:
    PeerDescriptor localPeerDescriptor = createMockPeerDescriptor();
    FakeTransport transport{
        localPeerDescriptor, [](const auto& /*message*/) {}};
    ListeningRpcCommunicator rpcCommunicator{ServiceID{"mock"}, transport};
    NodeList neighbors{
        Identifiers::getNodeIdFromPeerDescriptor(localPeerDescriptor),
        neighborTargetCount + 1};
    NodeList nearbyNodeView{
        Identifiers::getNodeIdFromPeerDescriptor(localPeerDescriptor),
        neighborTargetCount};
    MockNeighborFinder neighborFinder;
    std::set<DhtAddress> ongoingHandshakes;
    std::optional<NeighborUpdateRpcLocal> rpcLocal;

    void SetUp() override {
        this->rpcLocal.emplace(
            NeighborUpdateRpcLocalOptions{
                .localPeerDescriptor = this->localPeerDescriptor,
                .streamPartId = StreamPartIDUtils::parse("stream#0"),
                .neighbors = this->neighbors,
                .nearbyNodeView = this->nearbyNodeView,
                .neighborFinder = this->neighborFinder,
                .rpcCommunicator = this->rpcCommunicator,
                .neighborTargetCount = neighborTargetCount,
                .ongoingHandshakes = this->ongoingHandshakes});
    }

    void addNeighbors(size_t count) {
        for (size_t i = 0; i < count; i++) {
            ContentDeliveryRpcClient client{this->rpcCommunicator};
            this->neighbors.add(
                std::make_shared<ContentDeliveryRpcRemote>(
                    this->localPeerDescriptor,
                    createMockPeerDescriptor(),
                    client));
        }
    }

    static NeighborUpdate createUpdate(
        const std::vector<PeerDescriptor>& neighborDescriptors) {
        NeighborUpdate update;
        update.set_streampartid(StreamPartIDUtils::parse("stream#0"));
        for (const auto& descriptor : neighborDescriptors) {
            *update.add_neighbordescriptors() = descriptor;
        }
        update.set_removeme(false);
        return update;
    }

    static DhtCallContext createContext(const PeerDescriptor& sender) {
        DhtCallContext context;
        context.incomingSourceDescriptor = sender;
        return context;
    }
};

TEST_F(NeighborUpdateRpcLocalTest, ResponseContainsNeighborListOfExpectedSize) {
    this->addNeighbors(neighborTargetCount);
    const auto response = this->rpcLocal->neighborUpdate(
        createUpdate({this->localPeerDescriptor}),
        createContext(createMockPeerDescriptor()));
    EXPECT_EQ(response.neighbordescriptors().size(), neighborTargetCount);
}

TEST_F(NeighborUpdateRpcLocalTest, UpdatesContactsBasedOnCallersNeighbors) {
    this->addNeighbors(neighborTargetCount);
    EXPECT_EQ(this->nearbyNodeView.size(), 0);
    std::vector<PeerDescriptor> callerNeighbors;
    callerNeighbors.reserve(neighborTargetCount);
    for (size_t i = 0; i < neighborTargetCount; i++) {
        callerNeighbors.push_back(createMockPeerDescriptor());
    }
    this->rpcLocal->neighborUpdate(
        createUpdate(callerNeighbors),
        createContext(createMockPeerDescriptor()));
    EXPECT_EQ(this->nearbyNodeView.size(), 4);
}

TEST_F(NeighborUpdateRpcLocalTest, DoesNotAskToBeRemovedIfCallerIsNeighbor) {
    const auto caller = createMockPeerDescriptor();
    ContentDeliveryRpcClient client{this->rpcCommunicator};
    this->neighbors.add(
        std::make_shared<ContentDeliveryRpcRemote>(
            this->localPeerDescriptor, caller, client));
    const auto response = this->rpcLocal->neighborUpdate(
        createUpdate({this->localPeerDescriptor}), createContext(caller));
    EXPECT_EQ(response.removeme(), false);
}

TEST_F(NeighborUpdateRpcLocalTest, AsksToBeRemovedIfCallerIsNotNeighbor) {
    const auto caller = createMockPeerDescriptor();
    const auto response = this->rpcLocal->neighborUpdate(
        createUpdate({this->localPeerDescriptor}), createContext(caller));
    EXPECT_EQ(response.removeme(), true);
}

TEST_F(
    NeighborUpdateRpcLocalTest,
    AsksToBeRemovedIfCallerIsNeighborAndBothHaveTooManyNeighbors) {
    const auto caller = createMockPeerDescriptor();
    ContentDeliveryRpcClient client{this->rpcCommunicator};
    this->neighbors.add(
        std::make_shared<ContentDeliveryRpcRemote>(
            this->localPeerDescriptor, caller, client));
    this->addNeighbors(neighborTargetCount);
    std::vector<PeerDescriptor> callerNeighbors{this->localPeerDescriptor};
    for (size_t i = 0; i < neighborTargetCount; i++) {
        callerNeighbors.push_back(createMockPeerDescriptor());
    }
    const auto response = this->rpcLocal->neighborUpdate(
        createUpdate(callerNeighbors), createContext(caller));
    EXPECT_EQ(response.removeme(), true);
    EXPECT_EQ(
        this->neighbors.has(Identifiers::getNodeIdFromPeerDescriptor(caller)),
        false);
}

TEST_F(
    NeighborUpdateRpcLocalTest,
    DoesNotAskToBeRemovedIfOngoingHandshakeToCaller) {
    const auto caller = createMockPeerDescriptor();
    this->ongoingHandshakes.insert(
        Identifiers::getNodeIdFromPeerDescriptor(caller));
    const auto response = this->rpcLocal->neighborUpdate(
        createUpdate({this->localPeerDescriptor}), createContext(caller));
    EXPECT_EQ(response.removeme(), false);
}
