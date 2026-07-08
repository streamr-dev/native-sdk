// Module streamr.dht.TestUtils
// Test-only helpers shared by this package's test targets. Ported from
// packages/dht/test/utils/utils.ts and packages/dht/test/utils/
// customMatchers.ts (v103.8.0-rc.3); only the small factories the
// current tests need are here — the larger ones (createMockConnection-
// DhtNode etc.) are ported by the plan phase that first needs them
// (trackerless-network-completion-plan.md, phase 0.2).
module;

#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/proto-rpc/protos/ProtoRpc.pb.h"

export module streamr.dht.TestUtils;

import std;

import streamr.dht.Identifiers;
import streamr.utils.Uuid;

using streamr::utils::Uuid;

export namespace streamr::dht::testutils {

using ::dht::ClosestPeersRequest;
using ::dht::ConnectivityMethod;
using ::dht::DataEntry;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
using ::protorpc::RpcMessage;
using streamr::dht::DhtAddress;

// Ported from createMockPeerDescriptor() (test/utils/utils.ts). The TS
// version merges a Partial<PeerDescriptor> over the defaults; in C++ the
// caller mutates the returned descriptor instead.
inline PeerDescriptor createMockPeerDescriptor() {
    PeerDescriptor descriptor;
    descriptor.set_nodeid(
        Identifiers::getRawFromDhtAddress(
            Identifiers::createRandomDhtAddress()));
    descriptor.set_type(NodeType::NODEJS);
    return descriptor;
}

// Ported from createMockDataEntry() (test/utils/mock/mockDataEntry.ts). The
// data payload is left empty — the store tests identify entries by (key,
// creator), not by the packed data.
struct MockDataEntryOptions {
    std::optional<DhtAddress> key;
    std::optional<DhtAddress> creator;
    std::optional<std::uint32_t> ttl;
};

inline DataEntry createMockDataEntry(const MockDataEntryOptions& opts = {}) {
    constexpr std::uint32_t defaultTtl = 10000;
    DataEntry entry;
    entry.set_key(
        Identifiers::getRawFromDhtAddress(
            opts.key.value_or(Identifiers::createRandomDhtAddress())));
    entry.set_creator(
        Identifiers::getRawFromDhtAddress(
            opts.creator.value_or(Identifiers::createRandomDhtAddress())));
    entry.set_ttl(opts.ttl.value_or(defaultTtl));
    entry.set_stale(false);
    entry.set_deleted(false);
    const auto sinceEpoch = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(sinceEpoch);
    entry.mutable_createdat()->set_seconds(seconds.count());
    entry.mutable_createdat()->set_nanos(
        static_cast<std::int32_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                sinceEpoch - seconds)
                .count()));
    return entry;
}

// Ported from createWrappedClosestPeersRequest() (test/utils/utils.ts).
inline RpcMessage createWrappedClosestPeersRequest(
    const PeerDescriptor& sourceDescriptor) {
    ClosestPeersRequest routedMessage;
    routedMessage.set_nodeid(sourceDescriptor.nodeid());
    routedMessage.set_requestid(Uuid::v4());

    RpcMessage rpcWrapper;
    rpcWrapper.mutable_body()->PackFrom(routedMessage);
    (*rpcWrapper.mutable_header())["method"] = "closestPeersRequest";
    (*rpcWrapper.mutable_header())["request"] = "request";
    rpcWrapper.set_requestid(Uuid::v4());
    return rpcWrapper;
}

// Ported from customMatchers.ts. Jest extends expect() with
// toEqualPeerDescriptor; the gtest equivalent is a predicate-formatter
// used through EXPECT_PRED_FORMAT2, which produces the same per-field
// failure messages. (Not EXPECT_TRUE(result) on an AssertionResult:
// see the clangd note in ConnectionLockStatesTest.cpp.)
namespace detail {

inline std::string formErrorMessage(
    const std::string& field,
    const std::string& expected,
    const std::string& actual) {
    return "PeerDescriptor " + field +
        " values don't match:\nExpected: " + expected + "\nReceived: " + actual;
}

inline std::string toOutput(bool present, const ConnectivityMethod& method) {
    if (!present) {
        return "undefined";
    }
    std::ostringstream output;
    output << "{port: " << method.port() << ", host: '" << method.host()
           << "', tls: " << (method.tls() ? "true" : "false") << "}";
    return output.str();
}

inline void expectEqualConnectivityMethod(
    const std::string& field,
    bool expectedPresent,
    const ConnectivityMethod& expected,
    bool actualPresent,
    const ConnectivityMethod& actual,
    std::vector<std::string>& messages) {
    const bool equal = (expectedPresent == actualPresent) &&
        (!expectedPresent ||
         (expected.port() == actual.port() &&
          expected.host() == actual.host() && expected.tls() == actual.tls()));
    if (!equal) {
        messages.push_back(formErrorMessage(
            field,
            toOutput(expectedPresent, expected),
            toOutput(actualPresent, actual)));
    }
}

inline std::vector<std::string> collectPeerDescriptorDifferences(
    const PeerDescriptor& expected, const PeerDescriptor& actual) {
    std::vector<std::string> messages;
    if (expected.nodeid() != actual.nodeid()) {
        messages.push_back(formErrorMessage(
            "nodeId",
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{expected.nodeid()}),
            Identifiers::getDhtAddressFromRaw(DhtAddressRaw{actual.nodeid()})));
    }
    if (expected.type() != actual.type()) {
        const auto typeName = [](NodeType type) -> std::string {
            return type == NodeType::NODEJS ? "NODEJS" : "BROWSER";
        };
        messages.push_back(formErrorMessage(
            "type", typeName(expected.type()), typeName(actual.type())));
    }
    expectEqualConnectivityMethod(
        "udp",
        expected.has_udp(),
        expected.udp(),
        actual.has_udp(),
        actual.udp(),
        messages);
    expectEqualConnectivityMethod(
        "tcp",
        expected.has_tcp(),
        expected.tcp(),
        actual.has_tcp(),
        actual.tcp(),
        messages);
    expectEqualConnectivityMethod(
        "websocket",
        expected.has_websocket(),
        expected.websocket(),
        actual.has_websocket(),
        actual.websocket(),
        messages);
    if (expected.region() != actual.region()) {
        messages.push_back(formErrorMessage(
            "region",
            std::to_string(expected.region()),
            std::to_string(actual.region())));
    }
    return messages;
}

inline std::string joinMessages(const std::vector<std::string>& messages) {
    std::string joined;
    for (const auto& message : messages) {
        if (!joined.empty()) {
            joined += "\n\n";
        }
        joined += message;
    }
    return joined;
}

} // namespace detail

// Usage: EXPECT_PRED_FORMAT2(assertEqualPeerDescriptors, expected, actual)
inline testing::AssertionResult assertEqualPeerDescriptors(
    const char* /*expectedExpression*/,
    const char* /*actualExpression*/,
    const PeerDescriptor& expected,
    const PeerDescriptor& actual) {
    const auto messages =
        detail::collectPeerDescriptorDifferences(expected, actual);
    if (!messages.empty()) {
        return testing::AssertionFailure() << detail::joinMessages(messages);
    }
    return testing::AssertionSuccess();
}

// The equivalent of jest's .not.toEqualPeerDescriptor(...).
inline testing::AssertionResult assertNotEqualPeerDescriptors(
    const char* /*expectedExpression*/,
    const char* /*actualExpression*/,
    const PeerDescriptor& expected,
    const PeerDescriptor& actual) {
    const auto messages =
        detail::collectPeerDescriptorDifferences(expected, actual);
    if (messages.empty()) {
        return testing::AssertionFailure() << "PeerDescriptors are equal";
    }
    return testing::AssertionSuccess();
}

} // namespace streamr::dht::testutils
