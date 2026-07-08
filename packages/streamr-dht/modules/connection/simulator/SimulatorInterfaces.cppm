// Module streamr.dht.SimulatorInterfaces
// Interfaces that break the import cycle between Simulator,
// SimulatorConnection and SimulatorConnector (the TS originals import
// each other freely; C++ module imports must be acyclic, so the
// Simulator depends only on these interfaces and the concrete classes
// depend on the Simulator).
module;

#include <memory>
#include <optional>
#include <vector>

export module streamr.dht.SimulatorInterfaces;

import streamr.dht.protos;

export namespace streamr::dht::connection::simulator {

using ::dht::PeerDescriptor;

// The subset of SimulatorConnection the Simulator needs: delivery
// callbacks plus the peer descriptors whose regions drive the latency
// model. Connections are identified inside the Simulator by pointer
// identity (the TS version used connection ids for the same purpose).
class ISimulatorConnection {
protected:
    ISimulatorConnection() = default;

public:
    virtual ~ISimulatorConnection() = default;

    virtual void handleIncomingData(const std::vector<std::byte>& data) = 0;
    virtual void handleIncomingDisconnection() = 0;
    [[nodiscard]] virtual PeerDescriptor getLocalPeerDescriptor() const = 0;
    [[nodiscard]] virtual PeerDescriptor getRemotePeerDescriptor() const = 0;
};

// The subset of SimulatorConnector the Simulator needs to route an
// incoming connect operation to the target node.
class ISimulatorConnector {
protected:
    ISimulatorConnector() = default;

public:
    virtual ~ISimulatorConnector() = default;

    [[nodiscard]] virtual PeerDescriptor getPeerDescriptor() const = 0;
    virtual void handleIncomingConnection(
        const std::shared_ptr<ISimulatorConnection>& sourceConnection) = 0;
};

} // namespace streamr::dht::connection::simulator
