// Module streamr.dht.SimulatorConnection
// A Connection whose bytes travel through the in-process Simulator
// instead of a real socket. API ported from the TS SimulatorConnection
// (packages/dht/src/connection/simulator/SimulatorConnection.ts,
// v103.8.0-rc.3); adapted to this package's Connection base class and
// made thread-safe (the Simulator dispatcher thread and user threads
// both call in).
module;

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.SimulatorConnection;

import streamr.dht.Connection;
import streamr.dht.Identifiers;
import streamr.dht.Simulator;
import streamr.dht.SimulatorInterfaces;
import streamr.logger.SLogger;
import streamr.utils.EnableSharedFromThis;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;

export namespace streamr::dht::connection::simulator {

using ::dht::PeerDescriptor;
using streamr::dht::Identifiers;
using streamr::dht::connection::Connection;
using streamr::dht::connection::ConnectionType;

class SimulatorConnection : public Connection,
                            public ISimulatorConnection,
                            public EnableSharedFromThis {
private:
    struct Private {
        explicit Private() = default;
    };

    std::recursive_mutex mMutex;
    bool stopped = false;
    PeerDescriptor localPeerDescriptor;
    PeerDescriptor targetPeerDescriptor;
    Simulator& simulator;

    void doDisconnect(bool gracefulLeave) {
        {
            std::scoped_lock lock(this->mMutex);
            this->stopped = true;
        }
        SLogger::trace("doDisconnect() emitting");
        this->emit<connectionevents::Disconnected>(gracefulLeave, 0, "");
    }

public:
    SimulatorConnection(
        Private /*signal that constructor is private*/,
        PeerDescriptor localPeerDescriptor,
        PeerDescriptor targetPeerDescriptor,
        ConnectionType connectionType,
        Simulator& simulator)
        : Connection(connectionType),
          localPeerDescriptor(std::move(localPeerDescriptor)),
          targetPeerDescriptor(std::move(targetPeerDescriptor)),
          simulator(simulator) {}

    [[nodiscard]] static std::shared_ptr<SimulatorConnection> newInstance(
        PeerDescriptor localPeerDescriptor,
        PeerDescriptor targetPeerDescriptor,
        ConnectionType connectionType,
        Simulator& simulator) {
        return std::make_shared<SimulatorConnection>(
            Private{},
            std::move(localPeerDescriptor),
            std::move(targetPeerDescriptor),
            connectionType,
            simulator);
    }

    void send(const std::vector<std::byte>& data) override {
        SLogger::trace("send()");
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped) {
                SLogger::error("tried to send() on a stopped connection");
                return;
            }
        }
        this->simulator.send(*this, data);
    }

    void close(bool gracefulLeave) override {
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped) {
                SLogger::trace("close() tried to close a stopped connection");
                return;
            }
            this->stopped = true;
        }
        this->simulator.close(*this);
        this->doDisconnect(gracefulLeave);
    }

    void connect() {
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped) {
                SLogger::trace("tried to connect() a stopped connection");
                return;
            }
        }
        auto self = this->sharedFromThis<SimulatorConnection>();
        this->simulator.connect(
            self,
            this->targetPeerDescriptor,
            [self](const std::optional<std::string>& error) {
                if (error.has_value()) {
                    SLogger::trace(error.value());
                    self->doDisconnect(false);
                } else {
                    self->emit<connectionevents::Connected>();
                }
            });
    }

    void handleIncomingData(const std::vector<std::byte>& data) override {
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped) {
                SLogger::trace(
                    "tried to call handleIncomingData() on a stopped"
                    " connection");
                return;
            }
        }
        this->emit<connectionevents::Data>(data);
    }

    void handleIncomingDisconnection() override {
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped) {
                SLogger::trace(
                    "tried to call handleIncomingDisconnection() on a"
                    " stopped connection");
                return;
            }
        }
        SLogger::trace("handleIncomingDisconnection()");
        this->doDisconnect(false);
    }

    void destroy() override {
        {
            std::scoped_lock lock(this->mMutex);
            if (this->stopped) {
                SLogger::trace(
                    "tried to call destroy() on a stopped connection");
                return;
            }
        }
        this->removeAllListeners();
        this->close(false);
    }

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const override {
        return this->localPeerDescriptor;
    }

    [[nodiscard]] PeerDescriptor getRemotePeerDescriptor() const override {
        return this->targetPeerDescriptor;
    }
};

} // namespace streamr::dht::connection::simulator
