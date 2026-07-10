// Ported from packages/dht/test/unit/connectivityRequestHandler.test.ts
// (v103.8.0-rc.3): the handler is attached to a mock connection; a real
// websocket server on 127.0.0.1:15001 serves as the probe target. The happy
// path replies OPEN_INTERNET with the probed websocket info; with probing
// disabled (port 0) it replies UNKNOWN without websocket info.
#include <chrono>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.eventemitter.EventEmitter;
import streamr.utils.CoroutineHelper;
import streamr.utils.Ipv4Helper;
import streamr.utils.waitForCondition;
import streamr.dht.Connection;
import streamr.dht.connectivityChecker;
import streamr.dht.connectivityRequestHandler;
import streamr.dht.NatType;
import streamr.dht.Version;
import streamr.dht.WebsocketServer;
import streamr.dht.protos;

using ::dht::ConnectivityRequest;
using ::dht::Message;
using streamr::dht::connection::attachConnectivityRequestHandler;
using streamr::dht::connection::CONNECTIVITY_CHECKER_SERVICE_ID;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::connection::websocket::WebsocketServer;
using streamr::dht::connection::websocket::WebsocketServerConfig;
using streamr::dht::helpers::Version;
using streamr::utils::blockingWait;
using streamr::utils::Ipv4Helper;
using streamr::utils::waitForCondition;
using namespace std::chrono_literals;

namespace {

constexpr auto host = "127.0.0.1";
constexpr uint16_t port = 15001;
constexpr auto conditionTimeout = std::chrono::seconds(5);
constexpr auto conditionPollInterval = std::chrono::milliseconds(100);

// The TS test drives the handler with a bare EventEmitter carrying send()
// and getRemoteIpAddress(); this mock is its C++ equivalent (the handler is
// a template over the connection type).
class MockConnection : public streamr::eventemitter::EventEmitter<
                           streamr::dht::connection::ConnectionEvents> {
private:
    std::mutex mutex;
    std::vector<std::vector<std::byte>> sentData;

public:
    void send(const std::vector<std::byte>& data) {
        std::scoped_lock lock(this->mutex);
        this->sentData.push_back(data);
    }

    [[nodiscard]] std::string getRemoteAddress() const { // NOLINT
        return host;
    }

    [[nodiscard]] size_t getSentCount() {
        std::scoped_lock lock(this->mutex);
        return this->sentData.size();
    }

    [[nodiscard]] std::vector<std::byte> getSent(size_t index) {
        std::scoped_lock lock(this->mutex);
        return this->sentData.at(index);
    }
};

std::vector<std::byte> serializeMessage(const Message& message) {
    const auto serialized = message.SerializeAsString();
    std::vector<std::byte> data(serialized.size());
    std::memcpy(data.data(), serialized.data(), serialized.size());
    return data;
}

Message parseMessage(const std::vector<std::byte>& data) {
    Message message;
    message.ParseFromArray(data.data(), static_cast<int>(data.size()));
    return message;
}

} // namespace

class ConnectivityRequestHandlerTest : public ::testing::Test {
protected:
    std::optional<WebsocketServer> websocketServer;
    std::shared_ptr<MockConnection> connection;

    void SetUp() override {
        this->websocketServer.emplace(
            WebsocketServerConfig{
                .portRange = {.min = port, .max = port},
                .enableTls = false,
                .tlsCertificateFiles = std::nullopt,
                .maxMessageSize = std::nullopt});
        this->websocketServer->start();
        this->connection = std::make_shared<MockConnection>();
    }

    void TearDown() override { this->websocketServer->stop(); }

    [[nodiscard]] static Message createRequest(uint16_t requestPort) {
        Message request;
        request.set_serviceid(CONNECTIVITY_CHECKER_SERVICE_ID);
        request.set_messageid("mock-message-id");
        ConnectivityRequest* connectivityRequest =
            request.mutable_connectivityrequest();
        connectivityRequest->set_port(requestPort);
        connectivityRequest->set_host(host);
        connectivityRequest->set_tls(false);
        connectivityRequest->set_allowselfsignedcertificate(false);
        return request;
    }
};

TEST_F(ConnectivityRequestHandlerTest, HappyPath) {
    attachConnectivityRequestHandler(this->connection);
    this->connection->emit<Data>(serializeMessage(createRequest(port)));

    auto* rawConnection = this->connection.get();
    blockingWait(waitForCondition(
        [rawConnection]() { return rawConnection->getSentCount() > 0; },
        conditionTimeout,
        conditionPollInterval));

    const auto receivedMessage = parseMessage(this->connection->getSent(0));
    EXPECT_EQ(receivedMessage.serviceid(), CONNECTIVITY_CHECKER_SERVICE_ID);
    EXPECT_FALSE(receivedMessage.messageid().empty());
    ASSERT_TRUE(receivedMessage.has_connectivityresponse());
    const auto& response = receivedMessage.connectivityresponse();
    EXPECT_EQ(response.host(), host);
    EXPECT_EQ(response.nattype(), streamr::dht::types::NatType::OPEN_INTERNET);
    ASSERT_TRUE(response.has_websocket());
    EXPECT_EQ(response.websocket().host(), host);
    EXPECT_EQ(response.websocket().port(), port);
    EXPECT_FALSE(response.websocket().tls());
    EXPECT_EQ(response.ipaddress(), Ipv4Helper::ipv4ToNumber(host));
    EXPECT_EQ(response.protocolversion(), Version::localProtocolVersion);
}

TEST_F(ConnectivityRequestHandlerTest, DisabledConnectivityProbing) {
    attachConnectivityRequestHandler(this->connection);
    this->connection->emit<Data>(serializeMessage(createRequest(0)));

    auto* rawConnection = this->connection.get();
    blockingWait(waitForCondition(
        [rawConnection]() { return rawConnection->getSentCount() > 0; },
        conditionTimeout,
        conditionPollInterval));

    const auto receivedMessage = parseMessage(this->connection->getSent(0));
    EXPECT_EQ(receivedMessage.serviceid(), CONNECTIVITY_CHECKER_SERVICE_ID);
    ASSERT_TRUE(receivedMessage.has_connectivityresponse());
    const auto& response = receivedMessage.connectivityresponse();
    EXPECT_EQ(response.host(), host);
    EXPECT_EQ(response.nattype(), streamr::dht::types::NatType::UNKNOWN);
    EXPECT_FALSE(response.has_websocket());
    EXPECT_EQ(response.ipaddress(), Ipv4Helper::ipv4ToNumber(host));
    EXPECT_EQ(response.protocolversion(), Version::localProtocolVersion);
}
