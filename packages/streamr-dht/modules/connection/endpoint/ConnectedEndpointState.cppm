// Module streamr.dht.ConnectedEndpointState
// CONSOLIDATED from the former header
// streamr-dht/connection/endpoint/ConnectedEndpointState.hpp (MODERNIZATION.md
// Phase 2.6): this file is now the source of truth.
module;

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include <string>

export module streamr.dht.ConnectedEndpointState;

import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;
import streamr.dht.Connection;
import streamr.dht.EndpointState;
import streamr.dht.EndpointStateInterface;
import streamr.dht.Errors;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;
export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::helpers::SendFailed;

// The state's resources (connection, tokens) are guarded by the
// Endpoint's state-machine mutex (phase A0; see ConnectingEndpointState
// for the rationale). Methods overridden from EndpointState are called
// with that mutex held; the event handlers registered on the connection
// take it themselves and first validate that they still belong to the
// current connection.
class ConnectedEndpointState : public EndpointState {
private:
    EndpointStateInterface& stateInterface;
    std::shared_ptr<Connection> connection;
    HandlerToken dataHandlerToken;
    HandlerToken disconnectHandlerToken;

    explicit ConnectedEndpointState(EndpointStateInterface& stateInterface)
        : stateInterface(stateInterface) {
        SLogger::debug("ConnectedEndpointState constructor");
    }

    // Requires the state-machine mutex. Detaches the handlers from the
    // current connection and forgets it; a handler already running past
    // its off() no-ops on the identity check below.
    void detachFromConnection() {
        SLogger::debug("ConnectedEndpointState::detachFromConnection");
        if (this->connection) {
            this->connection->off<connectionevents::Data>(
                this->dataHandlerToken);
            this->connection->off<connectionevents::Disconnected>(
                this->disconnectHandlerToken);
            this->dataHandlerToken = HandlerToken{};
            this->disconnectHandlerToken = HandlerToken{};
            this->connection.reset();
        }
    }

public:
    [[nodiscard]] static std::shared_ptr<ConnectedEndpointState> newInstance(
        EndpointStateInterface& stateInterface) {
        struct MakeSharedEnabler : public ConnectedEndpointState {
            explicit MakeSharedEnabler(EndpointStateInterface& stateInterface)
                : ConnectedEndpointState(stateInterface) {}
        };
        return std::make_shared<MakeSharedEnabler>(stateInterface);
    }

    ~ConnectedEndpointState() override {
        SLogger::debug("ConnectedEndpointState destructor");
    }

    // Requires the state-machine mutex (called by
    // Endpoint::enterConnectedState). Unlike the pending connection, the
    // Connection emitter is fire-and-forget (no replay), so registering
    // the handlers here cannot invoke them synchronously — this is why
    // registration may stay inside the transition for this state.
    void enterState(
        const std::shared_ptr<Connection>& connection,
        const std::weak_ptr<void>& endpointPin) {
        SLogger::debug("ConnectedEndpointState::enterState start");
        this->detachFromConnection();
        this->connection = connection;
        std::weak_ptr<ConnectedEndpointState> weakSelf =
            this->sharedFromThis<ConnectedEndpointState>();
        auto* rawConnection = connection.get();
        this->dataHandlerToken = this->connection->on<connectionevents::Data>(
            [weakSelf, endpointPin, rawConnection](
                const std::vector<std::byte>& data) {
                // The pin keeps the Endpoint (and with it this state)
                // alive for the duration of the handler.
                const auto pin = endpointPin.lock();
                if (!pin) {
                    return;
                }
                const auto self = weakSelf.lock();
                if (!self) {
                    return;
                }
                {
                    std::scoped_lock lock(
                        self->stateInterface.getStateMachineMutex());
                    if (self->connection.get() != rawConnection) {
                        return; // stale handler of a replaced connection
                    }
                }
                self->stateInterface.emitData(data);
            });
        this->disconnectHandlerToken =
            this->connection->on<connectionevents::Disconnected>(
                [weakSelf, endpointPin, rawConnection](
                    bool /*gracefulLeave*/,
                    uint64_t /*code*/,
                    const std::string& /*reason*/) {
                    const auto pin = endpointPin.lock();
                    if (!pin) {
                        return;
                    }
                    const auto self = weakSelf.lock();
                    if (!self) {
                        return;
                    }
                    bool transitioned = false;
                    {
                        std::scoped_lock lock(
                            self->stateInterface.getStateMachineMutex());
                        if (self->connection.get() != rawConnection) {
                            return; // stale handler of a replaced connection
                        }
                        self->detachFromConnection();
                        transitioned =
                            self->stateInterface.enterDisconnectedState();
                    }
                    if (transitioned) {
                        self->stateInterface.finishDisconnect();
                    }
                });
        SLogger::debug("ConnectedEndpointState::enterState end");
    }

    [[nodiscard]] DeferredCallout close(bool graceful) override {
        SLogger::debug("ConnectedEndpointState::close");
        auto connection = this->connection;
        this->detachFromConnection();
        if (!connection) {
            return {};
        }
        // connection->close() emits its Disconnected event, so it must
        // run after the mutex is released.
        return [connection, graceful]() { connection->close(graceful); };
    }

    void send(const std::vector<std::byte>& data) override {
        SLogger::debug("ConnectedEndpointState::send");
        if (!this->connection) {
            throw SendFailed("send() called on endpoint with no connection");
        }
        this->connection->send(data);
    }

    [[nodiscard]] DeferredCallout changeToConnectingState(
        const std::shared_ptr<IPendingConnection>& pendingConnection) override {
        SLogger::debug("ConnectedEndpointState::changeToConnectingState");
        auto oldConnection = this->connection;
        this->detachFromConnection();
        this->stateInterface.enterConnectingState(pendingConnection);
        if (!oldConnection) {
            return {};
        }
        // Closing the replaced connection emits events; defer it.
        return [oldConnection]() { oldConnection->close(true); };
    }

    [[nodiscard]] DeferredCallout changeToConnectedState(
        const std::shared_ptr<Connection>& connection) override {
        SLogger::debug("ConnectedEndpointState::changeToConnectedState");
        // The replaced connection is handed off, not closed (matches the
        // pre-A0 behavior); enterConnectedState re-enters this state
        // with the new connection.
        this->stateInterface.enterConnectedState(connection);
        return {};
    }

    [[nodiscard]] bool isConnected() const override {
        SLogger::debug("ConnectedEndpointState::isConnected");
        return true;
    }
};

} // namespace streamr::dht::connection::endpoint
