// Module streamr.dht.EndpointStateInterface
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/EndpointStateInterface.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;


export module streamr.dht.EndpointStateInterface;

import std;

import streamr.dht.Connection;
import streamr.dht.IPendingConnection;
export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::IPendingConnection;

// Pure abstract callback interface through which the endpoint state
// classes drive the state machine. Endpoint implements it. Keeping this
// abstract — instead of the former concrete forwarder that held an
// Endpoint& and had its member definitions at the bottom of
// Endpoint.hpp — makes the endpoint header cluster acyclic: Endpoint
// depends on the states and the states depend only on this interface.
// (This was the only header cycle in the monorepo; the planned module
// consolidation needs the header graph to be a DAG.)
//
// LOCKING CONTRACT (phase A0 of trackerless-network-completion-plan.md):
// the whole state machine — the current-state pointer and every
// state-owned resource (pending connection, connection, handler tokens,
// send buffer) — is guarded by ONE mutex, owned by Endpoint and reached
// through getStateMachineMutex(). The former per-state mutexes are gone:
// they formed an ABBA pair with the endpoint mutex between a sending
// thread and the connection-event dispatcher thread. The methods below
// are split by that contract:
//  - the enter*State() transitions REQUIRE the state-machine mutex to be
//    held by the caller and perform no call-outs;
//  - emitData(), emitConnected() and finishDisconnect() perform the
//    call-outs (event emits, container removal) and must be called with
//    the state-machine mutex NOT held: a handler of those events may
//    call back into the state machine from another thread, so emitting
//    under the mutex deadlocks against the emitter's per-event emit-loop
//    mutex.
class EndpointStateInterface {
public:
    virtual ~EndpointStateInterface() = default;

    // The single state-machine mutex. State event handlers lock it
    // before validating that they are still relevant and transitioning.
    virtual std::recursive_mutex& getStateMachineMutex() = 0;

    // Transitions: caller must hold the state-machine mutex.
    virtual void enterConnectingState(
        const std::shared_ptr<IPendingConnection>& pendingConnection) = 0;
    virtual void enterConnectedState(
        const std::shared_ptr<Connection>& connection) = 0;
    // Returns false if the machine already was in the disconnected state,
    // so the disconnect call-outs run exactly once.
    virtual bool enterDisconnectedState() = 0;

    // Call-outs: caller must NOT hold the state-machine mutex.
    virtual void emitData(const std::vector<std::byte>& data) = 0;
    virtual void emitConnected() = 0;
    // Emits the Disconnected event and removes the endpoint from its
    // container; the final step of every disconnect/close path.
    virtual void finishDisconnect() = 0;
};

} // namespace streamr::dht::connection::endpoint
