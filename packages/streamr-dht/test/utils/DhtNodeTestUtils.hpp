// Ported from packages/dht/test/utils/utils.ts (v103.8.0-rc.3): helpers that
// build a DhtNode over a SimulatorTransport, and a layer-1 DhtNode over a
// layer-0 DhtNode.
//
// This is a plain header, NOT a module: a module that imports DhtNode's
// aggregated interface exhausts clang's source-location space during BMI
// generation ("ran out of source locations"). A .cpp that imports the modules
// and includes this header compiles to a .o without that BMI step. The
// including translation unit must therefore FIRST import:
//   streamr.utils.CoroutineHelper, streamr.dht.DhtNode,
//   streamr.dht.Identifiers, streamr.dht.protos, streamr.dht.RegionPings,
//   streamr.dht.Simulator, streamr.dht.SimulatorTransport
// TS subclasses DhtNode to override stop() to also stop the mock transport;
// here the returned MockDhtNode owns the transport and stopNode() stops both
// (a layer-1 node shares the layer-0 node's transport, so it has none).
#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace streamr::dht::testutils {

struct MockDhtNode {
    std::shared_ptr<streamr::dht::connection::simulator::SimulatorTransport>
        transport; // null for a layer-1 node
    std::shared_ptr<streamr::dht::DhtNode> node;

    void stopNode() {
        this->node->stop();
        if (this->transport != nullptr) {
            this->transport->stop();
        }
    }
};

inline std::shared_ptr<MockDhtNode> createMockConnectionDhtNode(
    streamr::dht::connection::simulator::Simulator& simulator,
    std::optional<streamr::dht::DhtAddress> nodeId = std::nullopt) {
    ::dht::PeerDescriptor peerDescriptor;
    peerDescriptor.set_nodeid(
        streamr::dht::Identifiers::getRawFromDhtAddress(nodeId.value_or(
            streamr::dht::Identifiers::createRandomDhtAddress())));
    peerDescriptor.set_type(::dht::NodeType::NODEJS);
    peerDescriptor.set_region(
        static_cast<uint32_t>(
            streamr::dht::connection::simulator::getRandomRegion()));
    auto transport = std::make_shared<
        streamr::dht::connection::simulator::SimulatorTransport>(
        peerDescriptor, simulator);
    transport->start();
    auto node =
        std::make_shared<streamr::dht::DhtNode>(streamr::dht::DhtNodeOptions{
            .rpcRequestTimeout = std::chrono::milliseconds(5000),
            .transport = transport.get(),
            .connectionsView = transport.get(),
            .connectionLocker = transport.get(),
            .peerDescriptor = peerDescriptor});
    streamr::utils::blockingWait(node->start());
    return std::make_shared<MockDhtNode>(MockDhtNode{transport, node});
}

inline std::shared_ptr<MockDhtNode> createMockConnectionLayer1Node(
    streamr::dht::DhtNode& layer0Node,
    const std::string& serviceId = "layer1") {
    ::dht::PeerDescriptor descriptor;
    descriptor.set_nodeid(layer0Node.getLocalPeerDescriptor().nodeid());
    descriptor.set_type(::dht::NodeType::NODEJS);
    auto node =
        std::make_shared<streamr::dht::DhtNode>(streamr::dht::DhtNodeOptions{
            .serviceId = streamr::dht::ServiceID{serviceId},
            .rpcRequestTimeout = std::chrono::milliseconds(10000),
            .transport = &layer0Node,
            .connectionsView = layer0Node.getConnectionsView(),
            .peerDescriptor = descriptor});
    streamr::utils::blockingWait(node->start());
    return std::make_shared<MockDhtNode>(MockDhtNode{nullptr, node});
}

} // namespace streamr::dht::testutils
