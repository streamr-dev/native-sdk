// Module streamr.dht.SimulatorConnector
// Creates simulator connections for one node. Ported from the TS
// SimulatorConnector
// (packages/dht/src/connection/simulator/SimulatorConnector.ts,
// v103.8.0-rc.3), adapted to this package's redesigned connection layer:
// outgoing/incoming connections are tracked as handshakers exactly like
// WebsocketClientConnector / WebsocketServerConnector do (the design-
// deviation rule of trackerless-network-completion-plan.md: the C++
// connection architecture wins, the TS behavior is preserved).
module;

#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include <string>

export module streamr.dht.SimulatorConnector;

import streamr.dht.protos;

import streamr.dht.Connection;
import streamr.dht.Handshaker;
import streamr.dht.Identifiers;
import streamr.dht.IncomingHandshaker;
import streamr.dht.IPendingConnection;
import streamr.dht.OutgoingHandshaker;
import streamr.dht.PendingConnection;
import streamr.dht.Simulator;
import streamr.dht.SimulatorConnection;
import streamr.dht.SimulatorInterfaces;
import streamr.logger.SLogger;
import streamr.utils.Uuid;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
using streamr::utils::Uuid;

export namespace streamr::dht::connection::simulator {

using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionType;
using streamr::dht::connection::IncomingHandshaker;
using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::OutgoingHandshaker;
using streamr::dht::connection::PendingConnection;

class SimulatorConnector : public ISimulatorConnector {
private:
    PeerDescriptor localPeerDescriptor;
    Simulator& simulator;
    std::function<bool(const std::shared_ptr<IPendingConnection>&)>
        onNewConnection;
    std::map<DhtAddress, std::shared_ptr<OutgoingHandshaker>>
        connectingHandshakers;
    std::map<std::string, std::shared_ptr<IncomingHandshaker>>
        incomingHandshakers;
    bool stopped = false;
    std::recursive_mutex mMutex;

public:
    SimulatorConnector(
        PeerDescriptor localPeerDescriptor,
        Simulator& simulator,
        std::function<bool(const std::shared_ptr<IPendingConnection>&)>
            onNewConnection)
        : localPeerDescriptor(std::move(localPeerDescriptor)),
          simulator(simulator),
          onNewConnection(std::move(onNewConnection)) {}

    std::shared_ptr<IPendingConnection> connect(
        const PeerDescriptor& targetPeerDescriptor,
        std::optional<std::function<void(std::exception_ptr)>> errorCallback =
            std::nullopt) {
        std::scoped_lock lock(this->mMutex);
        const auto nodeId =
            Identifiers::getNodeIdFromPeerDescriptor(targetPeerDescriptor);
        SLogger::trace("connect() " + nodeId);
        const auto existingHandshaker =
            this->connectingHandshakers.find(nodeId);
        if (existingHandshaker != this->connectingHandshakers.end()) {
            return existingHandshaker->second->getPendingConnection();
        }

        auto connection = SimulatorConnection::newInstance(
            this->localPeerDescriptor,
            targetPeerDescriptor,
            ConnectionType::SIMULATOR_CLIENT,
            this->simulator);

        auto pendingConnection = PendingConnection::newInstance(
            targetPeerDescriptor, std::move(errorCallback));

        auto outgoingHandshaker = OutgoingHandshaker::newInstance(
            this->localPeerDescriptor,
            connection,
            targetPeerDescriptor,
            pendingConnection);
        this->connectingHandshakers.emplace(nodeId, outgoingHandshaker);

        outgoingHandshaker->on<handshakerevents::HandshakerStopped>(
            [this, nodeId]() {
                std::scoped_lock lock(this->mMutex);
                if (this->connectingHandshakers.contains(nodeId)) {
                    this->connectingHandshakers.erase(nodeId);
                }
            });

        connection->connect();
        return pendingConnection;
    }

    [[nodiscard]] PeerDescriptor getPeerDescriptor() const override {
        return this->localPeerDescriptor;
    }

    void handleIncomingConnection(
        const std::shared_ptr<ISimulatorConnection>& sourceConnection)
        override {
        // the connection is incoming, so the remote peer descriptor is
        // the source connection's LOCAL peer descriptor
        const auto remotePeerDescriptor =
            sourceConnection->getLocalPeerDescriptor();
        SLogger::trace(
            Identifiers::getNodeIdFromPeerDescriptor(remotePeerDescriptor) +
            " incoming connection");
        std::shared_ptr<SimulatorConnection> connection;
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped) {
                return;
            }
            connection = SimulatorConnection::newInstance(
                this->localPeerDescriptor,
                remotePeerDescriptor,
                ConnectionType::SIMULATOR_SERVER,
                this->simulator);

            const auto handshakerId = Uuid::v4();
            auto handshaker = IncomingHandshaker::newInstance(
                this->localPeerDescriptor,
                connection,
                [](const DhtAddress& /*nodeId*/)
                    -> std::shared_ptr<IPendingConnection> {
                    // unlike the websocket server connector, the
                    // simulator has no RPC-mediated connect requests to
                    // pair incoming handshakes with
                    return nullptr;
                },
                this->onNewConnection);
            this->incomingHandshakers.emplace(
                handshakerId, std::move(handshaker));
            this->incomingHandshakers.at(handshakerId)
                ->on<handshakerevents::HandshakerStopped>(
                    [this, handshakerId]() {
                        std::scoped_lock lock(this->mMutex);
                        if (this->incomingHandshakers.contains(handshakerId)) {
                            this->incomingHandshakers.erase(handshakerId);
                        }
                    });
        }
        this->simulator.accept(sourceConnection, connection);
    }

    void stop() {
        std::scoped_lock lock(this->mMutex);
        this->stopped = true;
        for (const auto& [_, handshaker] : this->connectingHandshakers) {
            handshaker->getPendingConnection()->close(true);
        }
        this->connectingHandshakers.clear();
        this->incomingHandshakers.clear();
    }
};

} // namespace streamr::dht::connection::simulator
