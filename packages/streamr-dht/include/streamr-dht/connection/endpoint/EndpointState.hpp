#ifndef STREAMR_DHT_CONNECTION_ENDPOINT_ENDPOINTSTATE_HPP
#define STREAMR_DHT_CONNECTION_ENDPOINT_ENDPOINTSTATE_HPP

#include <cstddef>
#include <memory>
#include <vector>
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/EnableSharedFromThis.hpp"

namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::Connection;
using streamr::dht::connection::PendingConnection;
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;

class EndpointState : public EnableSharedFromThis {
protected:
    EndpointState() { SLogger::debug("EndpointState constructor"); }

public:
    ~EndpointState() override { SLogger::debug("EndpointState destructor"); }

    virtual void close(bool /*graceful*/) {
        SLogger::debug("EndpointState::close start");
        SLogger::debug("EndpointState::close end");
    };
    virtual void send(const std::vector<std::byte>& /* data */) {
        SLogger::debug("EndpointState::send start");
        SLogger::debug("EndpointState::send end");
    };

    virtual void changeToConnectingState(
        const std::shared_ptr<PendingConnection>& pendingConnection) = 0;

    virtual void changeToConnectedState(
        const std::shared_ptr<Connection>& connection) = 0;

    [[nodiscard]] virtual bool isConnected() const = 0;
};

} // namespace streamr::dht::connection::endpoint

#endif // STREAMR_DHT_CONNECTION_ENDPOINT_ENDPOINTSTATE_HPP
