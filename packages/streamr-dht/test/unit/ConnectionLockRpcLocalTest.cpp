#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

import streamr.dht.ConnectionLockRpcLocal;
import streamr.dht.ConnectionLockStates;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;

using ::dht::NodeType;
using ::dht::PeerDescriptor;
using ::dht::SetPrivateRequest;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionLockRpcLocal; // NOLINT
using streamr::dht::connection::ConnectionLockRpcLocalOptions;
using streamr::dht::rpcprotocol::DhtCallContext;

namespace {

PeerDescriptor createPeerDescriptor(const std::string& nodeId) {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(nodeId);
    descriptor.set_type(NodeType::NODEJS);
    return descriptor;
}

} // namespace

TEST(ConnectionLockRpcLocal, ItCanBeConstructed) {}

TEST(ConnectionLockRpcLocal, SetPrivateForwardsSenderIdAndFlag) {
    std::vector<std::pair<DhtAddress, bool>> calls;
    ConnectionLockRpcLocal rpcLocal{ConnectionLockRpcLocalOptions{
        .setPrivate = [&calls](const DhtAddress& id, bool isPrivate) {
            calls.emplace_back(id, isPrivate);
        }}};

    const auto sender = createPeerDescriptor("1234");
    DhtCallContext context;
    context.incomingSourceDescriptor = sender;

    SetPrivateRequest request;
    request.set_isprivate(true);
    rpcLocal.setPrivate(request, context);

    request.set_isprivate(false);
    rpcLocal.setPrivate(request, context);

    const auto expectedId = Identifiers::getNodeIdFromPeerDescriptor(sender);
    ASSERT_EQ(calls.size(), 2);
    EXPECT_EQ(calls[0].first, expectedId);
    EXPECT_TRUE(calls[0].second);
    EXPECT_EQ(calls[1].first, expectedId);
    EXPECT_FALSE(calls[1].second);
}
