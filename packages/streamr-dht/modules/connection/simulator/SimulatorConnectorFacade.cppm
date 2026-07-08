// Module streamr.dht.SimulatorConnectorFacade
// ConnectorFacade backed by the in-process Simulator, so a
// ConnectionManager runs unmodified on top of simulated connections.
// Ported from the TS SimulatorConnectorFacade
// (packages/dht/src/connection/ConnectorFacade.ts, v103.8.0-rc.3).
module;
#include <exception>

#include <functional>
#include <memory>
#include <utility>

export module streamr.dht.SimulatorConnectorFacade;

import streamr.dht.protos;

import streamr.dht.ConnectorFacade;
import streamr.dht.Identifiers;
import streamr.dht.IPendingConnection;
import streamr.dht.Simulator;
import streamr.dht.SimulatorConnector;
import streamr.logger.SLogger;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;

export namespace streamr::dht::connection::simulator {

using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectorFacade;
using streamr::dht::connection::IPendingConnection;

class SimulatorConnectorFacade : public ConnectorFacade {
private:
    PeerDescriptor localPeerDescriptor;
    Simulator& simulator;
    std::shared_ptr<SimulatorConnector> simulatorConnector;

public:
    SimulatorConnectorFacade(
        PeerDescriptor localPeerDescriptor, Simulator& simulator)
        : localPeerDescriptor(std::move(localPeerDescriptor)),
          simulator(simulator) {}

    void start(
        std::function<bool(const std::shared_ptr<IPendingConnection>&)>
            onNewConnection,
        std::function<bool(const DhtAddress& nodeId)> /*hasConnection*/)
        override {
        SLogger::trace("Creating SimulatorConnector");
        this->simulatorConnector = std::make_shared<SimulatorConnector>(
            this->localPeerDescriptor,
            this->simulator,
            std::move(onNewConnection));
        this->simulator.addConnector(this->simulatorConnector);
    }

    std::shared_ptr<IPendingConnection> createConnection(
        const PeerDescriptor& peerDescriptor) override {
        return this->simulatorConnector->connect(peerDescriptor);
    }

    std::shared_ptr<IPendingConnection> createConnection(
        const PeerDescriptor& peerDescriptor,
        std::function<void(std::exception_ptr)> errorCallback) override {
        return this->simulatorConnector->connect(
            peerDescriptor, std::move(errorCallback));
    }

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const override {
        return this->localPeerDescriptor;
    }

    void stop() override { this->simulatorConnector->stop(); }
};

} // namespace streamr::dht::connection::simulator
