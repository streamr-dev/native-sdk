// Module streamr.dht.SimulatorTransport
// A ConnectionManager whose connections run through the in-process
// Simulator — the transport used by simulator-based integration tests.
// Ported from the TS SimulatorTransport
// (packages/dht/src/connection/simulator/SimulatorTransport.ts,
// v103.8.0-rc.3).
module;


export module streamr.dht.SimulatorTransport;

import std;

import streamr.dht.protos;

import streamr.dht.ConnectionManager;
import streamr.dht.ConnectorFacade;
import streamr.dht.Simulator;
import streamr.dht.SimulatorConnectorFacade;

export namespace streamr::dht::connection::simulator {

using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::ConnectorFacade;

class SimulatorTransport : public ConnectionManager {
public:
    // Same maxConnections default as the TS ConnectionManager applies
    // when SimulatorTransport passes none.
    static constexpr std::size_t defaultMaxConnections = 80;

    SimulatorTransport(
        const PeerDescriptor& localPeerDescriptor, Simulator& simulator)
        : ConnectionManager(
              ConnectionManagerOptions{
                  .maxConnections = defaultMaxConnections,
                  .createConnectorFacade = [localPeerDescriptor, &simulator]()
                      -> std::shared_ptr<ConnectorFacade> {
                      return std::make_shared<SimulatorConnectorFacade>(
                          localPeerDescriptor, simulator);
                  },
                  .allowIncomingPrivateConnections = false}) {}
};

} // namespace streamr::dht::connection::simulator
