// Module streamr.dht.connectivityRequestHandler
// Ported from packages/dht/src/connection/connectivityRequestHandler.ts
// (v103.8.0-rc.3): the server side of the NAT connectivity check. Attached to
// an incoming websocket connection whose URL carried the connectivityRequest
// action; on a ConnectivityRequest it probes the requester's advertised
// websocket server (connect back to host:port, action=connectivityProbe)
// unless probing is disabled (port 0), and replies with a
// ConnectivityResponse. The GeoIP location lookup is omitted (deferred to
// milestone E with the rest of GeoIP).
//
// The handling runs on a dedicated worker executor (magic static, like
// AbortableTimers'): the probe blocks up to the connect timeout, which must
// not stall the websocket dispatch thread the data event fires on.
// attachConnectivityRequestHandler is a template over the connection type so
// the unit test can drive it with a mock connection (the TS test does the
// same with a bare EventEmitter); the real caller passes the
// WebsocketServerConnection.
module;

#include <memory>
#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <folly/executors/CPUThreadPoolExecutor.h>

export module streamr.dht.connectivityRequestHandler;

import streamr.dht.protos;

import streamr.dht.Connection;
import streamr.dht.Connectivity;
import streamr.dht.connectivityChecker;
import streamr.dht.NatType;
import streamr.dht.Version;
import streamr.logger.SLogger;
import streamr.utils.Ipv4Helper;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::Ipv4Helper;

export namespace streamr::dht::connection {

using ::dht::ConnectivityRequest;
using ::dht::ConnectivityResponse;
using ::dht::Message;
using streamr::dht::helpers::Connectivity;
using streamr::dht::helpers::Version;

inline constexpr uint32_t DISABLE_CONNECTIVITY_PROBE = 0;

namespace connectivityrequesthandlerdetail {

inline folly::CPUThreadPoolExecutor& getHandlerExecutor() {
    // magic static
    static folly::CPUThreadPoolExecutor executor{1};
    return executor;
}

inline ConnectivityResponse connectivityProbe(
    const ConnectivityRequest& connectivityRequest,
    const std::string& ipAddress,
    const std::string& host) {
    ConnectivityResponse response;
    ::dht::ConnectivityMethod wsServerInfo;
    wsServerInfo.set_host(host);
    wsServerInfo.set_port(connectivityRequest.port());
    wsServerInfo.set_tls(connectivityRequest.tls());
    const auto url = Connectivity::connectivityMethodToWebsocketUrl(
        wsServerInfo, "connectivityProbe");
    SLogger::trace("Attempting Connectivity Check to " + url);
    try {
        auto outgoingConnection =
            connectSync(url, connectivityRequest.allowselfsignedcertificate());
        SLogger::trace(
            "Connectivity test produced positive result, communicating"
            " reply to the requester " +
            host + ":" + std::to_string(connectivityRequest.port()));
        response.set_host(host);
        response.set_nattype(types::NatType::OPEN_INTERNET);
        response.mutable_websocket()->set_host(host);
        response.mutable_websocket()->set_port(connectivityRequest.port());
        response.mutable_websocket()->set_tls(connectivityRequest.tls());
        response.set_ipaddress(Ipv4Helper::ipv4ToNumber(ipAddress));
        response.set_protocolversion(Version::localProtocolVersion);
        outgoingConnection->close(false);
    } catch (const std::exception& err) {
        SLogger::debug("error " + std::string(err.what()));
        response.set_host(host);
        response.set_nattype(types::NatType::UNKNOWN);
        response.set_ipaddress(Ipv4Helper::ipv4ToNumber(ipAddress));
        response.set_protocolversion(Version::localProtocolVersion);
    }
    return response;
}

template <typename ConnectionType>
inline void handleIncomingConnectivityRequest(
    ConnectionType& connection,
    const ConnectivityRequest& connectivityRequest) {
    const auto ipAddress = connection.getRemoteAddress();
    const auto host =
        connectivityRequest.has_host() ? connectivityRequest.host() : ipAddress;
    ConnectivityResponse connectivityResponse;
    if (connectivityRequest.port() != DISABLE_CONNECTIVITY_PROBE) {
        connectivityResponse =
            connectivityProbe(connectivityRequest, ipAddress, host);
    } else {
        SLogger::trace(
            "ConnectivityRequest port is 0, replying without"
            " connectivityProbe");
        connectivityResponse.set_host(host);
        connectivityResponse.set_nattype(types::NatType::UNKNOWN);
        connectivityResponse.set_ipaddress(Ipv4Helper::ipv4ToNumber(ipAddress));
        connectivityResponse.set_protocolversion(Version::localProtocolVersion);
    }
    Message msg;
    msg.set_serviceid(CONNECTIVITY_CHECKER_SERVICE_ID);
    msg.set_messageid(
        boost::uuids::to_string(boost::uuids::random_generator()()));
    *msg.mutable_connectivityresponse() = connectivityResponse;
    const auto serialized = msg.SerializeAsString();
    std::vector<std::byte> data(serialized.size());
    std::memcpy(data.data(), serialized.data(), serialized.size());
    connection.send(data);
    SLogger::trace("ConnectivityResponse sent");
}

} // namespace connectivityrequesthandlerdetail

template <typename ConnectionType>
inline void attachConnectivityRequestHandler(
    const std::shared_ptr<ConnectionType>& connectionToListenTo) {
    namespace detail = connectivityrequesthandlerdetail;
    std::weak_ptr<ConnectionType> weakConnection = connectionToListenTo;
    connectionToListenTo->template on<connectionevents::Data>(
        [weakConnection](const std::vector<std::byte>& data) {
            SLogger::trace("server received data");
            Message message;
            if (!message.ParseFromArray(
                    data.data(), static_cast<int>(data.size()))) {
                SLogger::trace("Could not parse message");
                return;
            }
            if (!message.has_connectivityrequest()) {
                return;
            }
            SLogger::trace("ConnectivityRequest received");
            detail::getHandlerExecutor().add(
                [weakConnection, request = message.connectivityrequest()]() {
                    const auto connection = weakConnection.lock();
                    if (!connection) {
                        return;
                    }
                    try {
                        detail::handleIncomingConnectivityRequest(
                            *connection, request);
                        SLogger::trace("handleIncomingConnectivityRequest ok");
                    } catch (const std::exception& err) {
                        SLogger::error(
                            "handleIncomingConnectivityRequest " +
                            std::string(err.what()));
                    }
                });
        });
}

} // namespace streamr::dht::connection
