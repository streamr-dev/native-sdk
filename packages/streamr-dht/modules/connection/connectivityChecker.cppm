// Module streamr.dht.connectivityChecker
// Ported from packages/dht/src/connection/connectivityChecker.ts
// (v103.8.0-rc.3): the client side of the NAT connectivity check. connectSync
// opens a raw websocket client connection and waits for it to come up (the TS
// connectAsync); sendConnectivityRequest connects to an entry point's
// websocket server with the connectivityRequest action, sends a
// ConnectivityRequest and waits for the ConnectivityResponse.
//
// Deliberately synchronous-blocking (a C++ design point): both entry points
// run on dedicated worker threads — the connectivity-request handler's
// executor on the server side, and the connector facade's start path on the
// client side — never on an event-dispatch or RPC-delivery thread. The
// event listeners are registered BEFORE connect()/send() fire, so a fast
// completion cannot be lost (the TS code gets the same ordering from its
// eager promises).
module;

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <folly/futures/Future.h>

export module streamr.dht.connectivityChecker;

import streamr.dht.protos;

import streamr.dht.Connection;
import streamr.dht.Connectivity;
import streamr.dht.Errors;
import streamr.dht.Version;
import streamr.dht.WebsocketClientConnection;
import streamr.logger.SLogger;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht::connection {

using ::dht::ConnectivityRequest;
using ::dht::ConnectivityResponse;
using ::dht::Message;
using ::dht::PeerDescriptor;
using streamr::dht::connection::websocket::WebsocketClientConnection;
using streamr::dht::helpers::ConnectionFailed;
using streamr::dht::helpers::Connectivity;
using streamr::dht::helpers::ConnectivityResponseTimeout;
using streamr::dht::helpers::Version;

inline constexpr auto CONNECTIVITY_CHECKER_SERVICE_ID =
    "system/connectivity-checker";
inline constexpr std::chrono::milliseconds connectivityCheckerTimeout{5000};
inline constexpr std::chrono::milliseconds connectDefaultTimeout{1000};

// TS connectAsync: open a websocket client connection to `url` and wait for
// it to be connected; throws ConnectionFailed on error or timeout. The
// returned connection is open; the caller closes it.
inline std::shared_ptr<WebsocketClientConnection> connectSync(
    const std::string& url,
    bool allowSelfSignedCertificate,
    std::chrono::milliseconds timeout = connectDefaultTimeout) {
    auto socket = WebsocketClientConnection::newInstance();
    // One shared promise raced by the connected and error listeners; the
    // flag makes the loser's later firing a no-op.
    auto promise = std::make_shared<folly::Promise<bool>>();
    auto settled = std::make_shared<std::atomic_flag>();
    socket->once<connectionevents::Connected>([promise, settled]() {
        if (!settled->test_and_set()) {
            promise->setValue(true);
        }
    });
    socket->once<connectionevents::Error>(
        [promise, settled](const std::string& /*name*/) {
            if (!settled->test_and_set()) {
                promise->setValue(false);
            }
        });
    auto future = promise->getFuture();
    socket->connect(url, allowSelfSignedCertificate);
    bool connected = false;
    try {
        connected = std::move(future).get(timeout);
    } catch (const folly::FutureTimeout&) {
        settled->test_and_set(); // silence late listeners
        socket->destroy();
        throw ConnectionFailed("WebSocket connection timed out");
    }
    if (!connected) {
        socket->destroy();
        throw ConnectionFailed("Could not open WebSocket connection");
    }
    return socket;
}

// TS sendConnectivityRequest: run the connectivity check against one entry
// point and return its ConnectivityResponse.
inline ConnectivityResponse sendConnectivityRequest(
    const ConnectivityRequest& request, const PeerDescriptor& entryPoint) {
    const auto url = Connectivity::connectivityMethodToWebsocketUrl(
        entryPoint.websocket(), "connectivityRequest");
    SLogger::debug("Attempting connectivity check with entrypoint " + url);
    std::shared_ptr<WebsocketClientConnection> outgoingConnection;
    try {
        outgoingConnection =
            connectSync(url, request.allowselfsignedcertificate());
    } catch (const std::exception& e) {
        throw ConnectionFailed(
            "Failed to connect to entrypoint for connectivity check: " + url,
            std::string(e.what()));
    }
    // Register the response listener BEFORE sending the request.
    auto promise = std::make_shared<folly::Promise<ConnectivityResponse>>();
    auto settled = std::make_shared<std::atomic_flag>();
    outgoingConnection->on<connectionevents::Data>(
        [promise, settled](const std::vector<std::byte>& bytes) {
            Message message;
            if (!message.ParseFromArray(
                    bytes.data(), static_cast<int>(bytes.size()))) {
                SLogger::trace("Could not parse message");
                return;
            }
            if (message.has_connectivityresponse() &&
                !settled->test_and_set()) {
                promise->setValue(message.connectivityresponse());
            }
        });
    Message msg;
    msg.set_serviceid(CONNECTIVITY_CHECKER_SERVICE_ID);
    msg.set_messageid(
        boost::uuids::to_string(boost::uuids::random_generator()()));
    *msg.mutable_connectivityrequest() = request;
    const auto serialized = msg.SerializeAsString();
    std::vector<std::byte> data(serialized.size());
    std::memcpy(data.data(), serialized.data(), serialized.size());
    auto future = promise->getFuture();
    outgoingConnection->send(data);
    SLogger::trace("ConnectivityRequest sent");
    ConnectivityResponse response;
    try {
        response = std::move(future).get(connectivityCheckerTimeout);
    } catch (const folly::FutureTimeout&) {
        settled->test_and_set();
        outgoingConnection->close(false);
        throw ConnectivityResponseTimeout("timeout");
    }
    outgoingConnection->close(false);
    if (!Version::isMaybeSupportedVersion(response.protocolversion())) {
        throw ConnectionFailed(
            "Unsupported version: " + response.protocolversion());
    }
    return response;
}

} // namespace streamr::dht::connection
