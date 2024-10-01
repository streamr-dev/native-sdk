
#ifndef STREAMR_DHT_CONNECTION_PENDINGCONNECTION_HPP
#define STREAMR_DHT_CONNECTION_PENDINGCONNECTION_HPP

#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-utils/AbortController.hpp"
#include "streamr-utils/AbortableTimers.hpp"

namespace streamr::dht::connection {

using utils::AbortController;
using utils::AbortableTimers;
using ::dht::PeerDescriptor;

class PendingConnection : public IPendingConnection {
private:
    AbortController connectingAbortController;
    PeerDescriptor remotePeerDescriptor;
    bool replacedAsDuplicate = false;
    bool stopped = false;

public:
    explicit PendingConnection(
        PeerDescriptor remotePeerDescriptor,
        std::chrono::milliseconds timeout =
            std::chrono::milliseconds(15 * 1000))
        : remotePeerDescriptor(std::move(remotePeerDescriptor)) {
        AbortableTimers::setAbortableTimeout(
            [this]() { this->close(false); },
            timeout,
            this->connectingAbortController.getSignal());
    }
    void onHandshakeCompleted(std::shared_ptr<Connection> connection) override {
        if (!this->replacedAsDuplicate) {
            this->emit<pendingconnectionevents::Connected>(
                this->remotePeerDescriptor, connection);
        }
    }

    void close(bool graceful) override {
        if (this->stopped) {
            return;
        }
        this->stopped = true;
        this->connectingAbortController.abort();
        if (!this->replacedAsDuplicate) {
            this->emit<pendingconnectionevents::Disconnected>(graceful);
        }
    }

    void destroy() override {
        if (this->stopped) {
            return;
        }
        this->stopped = true;
        this->connectingAbortController.abort();
        this->removeAllListeners();
    }

    void replaceAsDuplicate() { this->replacedAsDuplicate = true; }

    const PeerDescriptor& getPeerDescriptor() const override {
        return this->remotePeerDescriptor;
    }
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_PENDINGCONNECTION_HPP