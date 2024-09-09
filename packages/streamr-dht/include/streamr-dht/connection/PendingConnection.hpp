
#ifndef STREAMR_DHT_CONNECTION_PENDINGCONNECTION_HPP
#define STREAMR_DHT_CONNECTION_PENDINGCONNECTION_HPP

#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/Identifiers.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-utils/AbortController.hpp"
#include "streamr-utils/abortableTimers.hpp"

namespace streamr::dht::connection {

using ::dht::PeerDescriptor;
using streamr::dht::connection::Connection;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::utils::AbortController;
using streamr::utils::AbortableTimers;
using streamr::dht::Identifiers;

namespace pendingconnectionevents {

struct Connected : Event<const PeerDescriptor&, const std::shared_ptr<Connection>&> {};
struct Disconnected : Event<bool /*gracefulLeave*/> {};

} // namespace pendingconnectionevents

using PendingConnectionEvents = std::tuple<pendingconnectionevents::Connected, pendingconnectionevents::Disconnected>;

class PendingConnection : public EventEmitter<PendingConnectionEvents> {
private:
    AbortController connectingAbortController;
    PeerDescriptor remotePeerDescriptor;
    bool replacedAsDuplicate = false;
    bool stopped = false;

public:
    explicit PendingConnection(
        PeerDescriptor remotePeerDescriptor,
        std::chrono::milliseconds timeout =
            std::chrono::milliseconds(15 * 1000)) // NOLINT
        : remotePeerDescriptor(std::move(remotePeerDescriptor)) {
        AbortableTimers::setAbortableTimeout(
            [this]() { this->close(false); },
            timeout,
            this->connectingAbortController.signal);
    }

    void replaceAsDuplicate() {
        SLogger::trace(Identifiers::getNodeIdFromPeerDescriptor(this->remotePeerDescriptor) + " replaceAsDuplicate");
        this->replacedAsDuplicate = true;
    }

    void onHandshakeCompleted(const std::shared_ptr<Connection>& connection) {
        if (!this->replacedAsDuplicate) {
            this->emit<pendingconnectionevents::Connected>(this->remotePeerDescriptor, connection);
        }
    }

    void close(bool graceful) {
        if (this->stopped) {
            return;
        }
        this->stopped = true;
        this->connectingAbortController.abort();
        if (!this->replacedAsDuplicate) {
            this->emit<pendingconnectionevents::Disconnected>(graceful);
        }
    }

    void destroy() {
        if (this->stopped) {
            return;
        }
        this->stopped = true;
        this->connectingAbortController.abort();
        this->removeAllListeners();
    }

    [[nodiscard]] const PeerDescriptor& getPeerDescriptor() const {
        return this->remotePeerDescriptor;
    }
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_PENDINGCONNECTION_HPP