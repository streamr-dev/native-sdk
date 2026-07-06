// Ported from packages/dht/test/unit/customMatchers.test.ts
// (v103.8.0-rc.3). Jest's expect().toEqualPeerDescriptor(...) becomes
// EXPECT_PRED_FORMAT2 with the assertion formatters from
// streamr.dht.TestUtils; jest's .toThrow(message) checks on the matcher
// become EXPECT_NONFATAL_FAILURE substring checks.
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

import streamr.dht.TestUtils;

using streamr::dht::testutils::assertEqualPeerDescriptors;
using streamr::dht::testutils::assertNotEqualPeerDescriptors;
using streamr::dht::testutils::createMockPeerDescriptor;

TEST(CustomMatchers, HappyPath) {
    auto peerDescriptor = createMockPeerDescriptor();
    peerDescriptor.mutable_websocket()->set_port(1);
    peerDescriptor.mutable_websocket()->set_host("x");
    peerDescriptor.mutable_websocket()->set_tls(true);
    const auto copy = peerDescriptor;
    EXPECT_PRED_FORMAT2(assertEqualPeerDescriptors, peerDescriptor, copy);
}

TEST(CustomMatchers, NoMatch) {
    EXPECT_PRED_FORMAT2(
        assertNotEqualPeerDescriptors,
        createMockPeerDescriptor(),
        createMockPeerDescriptor());
}

TEST(CustomMatchers, ErrorMessageNormal) {
    const auto actual = createMockPeerDescriptor();
    const auto expected = createMockPeerDescriptor();
    EXPECT_NONFATAL_FAILURE(
        EXPECT_PRED_FORMAT2(assertEqualPeerDescriptors, expected, actual),
        "PeerDescriptor nodeId values don't match");
}

TEST(CustomMatchers, ErrorMessageInverse) {
    const auto peerDescriptor = createMockPeerDescriptor();
    EXPECT_NONFATAL_FAILURE(
        EXPECT_PRED_FORMAT2(
            assertNotEqualPeerDescriptors, peerDescriptor, peerDescriptor),
        "PeerDescriptors are equal");
}
