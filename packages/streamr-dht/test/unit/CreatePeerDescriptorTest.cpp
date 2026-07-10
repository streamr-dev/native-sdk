// Ported from packages/dht/test/unit/createPeerDescriptor.test.ts
// (v103.8.0-rc.3). The TS expect.any(Uint8Array) assertions become size
// checks (the node id is always 20 bytes, the throwaway public key 20 bytes
// and the recoverable secp256k1 signature 65 bytes); there is no browser
// environment, so the type is always NODEJS.
#include <cstddef>
#include <string>
#include <gtest/gtest.h>

import streamr.dht.createPeerDescriptor;
import streamr.dht.Identifiers;
import streamr.dht.RegionPings;
import streamr.dht.protos;
import streamr.utils.Ipv4Helper;

using ::dht::ConnectivityResponse;
using ::dht::NodeType;
using streamr::dht::Identifiers;
using streamr::dht::connection::simulator::getRandomRegion;
using streamr::dht::helpers::createPeerDescriptor;
using streamr::utils::Ipv4Helper;

namespace {

constexpr auto ipAddressString = "1.2.3.4";
constexpr size_t nodeIdBytes = 20;
constexpr size_t publicKeyBytes = 20;
constexpr size_t signatureBytes = 65;

} // namespace

class CreatePeerDescriptorTest : public ::testing::Test {
protected:
    uint32_t region = static_cast<uint32_t>(getRandomRegion());
    uint32_t ipAddress = Ipv4Helper::ipv4ToNumber(ipAddressString);
};

TEST_F(CreatePeerDescriptorTest, WithoutWebsocket) {
    ConnectivityResponse connectivityResponse;
    connectivityResponse.set_ipaddress(this->ipAddress);
    const auto peerDescriptor =
        createPeerDescriptor(connectivityResponse, this->region);
    EXPECT_EQ(peerDescriptor.nodeid().size(), nodeIdBytes);
    EXPECT_EQ(peerDescriptor.type(), NodeType::NODEJS);
    EXPECT_EQ(peerDescriptor.ipaddress(), this->ipAddress);
    EXPECT_EQ(peerDescriptor.publickey().size(), publicKeyBytes);
    EXPECT_EQ(peerDescriptor.signature().size(), signatureBytes);
    EXPECT_EQ(peerDescriptor.region(), this->region);
    EXPECT_FALSE(peerDescriptor.has_websocket());
}

TEST_F(CreatePeerDescriptorTest, WithWebsocket) {
    ConnectivityResponse connectivityResponse;
    connectivityResponse.set_ipaddress(this->ipAddress);
    connectivityResponse.mutable_websocket()->set_host("bar.com");
    connectivityResponse.mutable_websocket()->set_port(123); // NOLINT
    connectivityResponse.mutable_websocket()->set_tls(true);
    const auto peerDescriptor =
        createPeerDescriptor(connectivityResponse, this->region);
    EXPECT_EQ(peerDescriptor.nodeid().size(), nodeIdBytes);
    EXPECT_EQ(peerDescriptor.type(), NodeType::NODEJS);
    EXPECT_EQ(peerDescriptor.ipaddress(), this->ipAddress);
    EXPECT_EQ(peerDescriptor.publickey().size(), publicKeyBytes);
    EXPECT_EQ(peerDescriptor.signature().size(), signatureBytes);
    ASSERT_TRUE(peerDescriptor.has_websocket());
    EXPECT_EQ(peerDescriptor.websocket().host(), "bar.com");
    EXPECT_EQ(peerDescriptor.websocket().port(), 123);
    EXPECT_TRUE(peerDescriptor.websocket().tls());
    EXPECT_EQ(peerDescriptor.region(), this->region);
}

TEST_F(CreatePeerDescriptorTest, ExplicitNodeId) {
    const auto nodeId = Identifiers::createRandomDhtAddress();
    ConnectivityResponse connectivityResponse;
    connectivityResponse.set_ipaddress(this->ipAddress);
    const auto peerDescriptor =
        createPeerDescriptor(connectivityResponse, this->region, nodeId);
    EXPECT_EQ(
        peerDescriptor.nodeid(), Identifiers::getRawFromDhtAddress(nodeId));
    EXPECT_EQ(peerDescriptor.type(), NodeType::NODEJS);
    EXPECT_EQ(peerDescriptor.ipaddress(), this->ipAddress);
    EXPECT_EQ(peerDescriptor.publickey().size(), publicKeyBytes);
    EXPECT_EQ(peerDescriptor.signature().size(), signatureBytes);
    EXPECT_EQ(peerDescriptor.region(), this->region);
}
