// Mirrors StreamrNodeTest.TwoNodesExchangeMessages through the C++
// wrapper (phase D3b): create/start/join/publish/subscribe round-trip
// between two wrapper-created nodes, plus the error mapping.

#include <algorithm>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <gtest/gtest.h>

#include "StreamrNode.hpp"

using streamrproxyclient::ErrorInvalidEthereumAddress;
using streamrproxyclient::ErrorSubscriptionNotFound;
using streamrproxyclient::StreamrNode;
using streamrproxyclient::StreamrNodeError;
using streamrproxyclient::StreamrNodeOptions;
using streamrproxyclient::StreamrProxyAddress;

namespace {

constexpr auto pollInterval = std::chrono::milliseconds(200);
constexpr auto topologyTimeout = std::chrono::seconds(60);
constexpr auto messageTimeout = std::chrono::seconds(30);
constexpr uint16_t entryPointPort = 44461;
constexpr const char* ethereumAddressA =
    "0x1234567890123456789012345678901234567890";
constexpr const char* ethereumAddressB =
    "0x1234567890123456789012345678901234567892";
constexpr const char* ethereumPrivateKey =
    "1111111111111111111111111111111111111111111111111111111111111111";
constexpr const char* streamPartId =
    "0xa000000000000000000000000000000000000000#01";

template <typename ConditionType>
bool waitUntil(const ConditionType& condition, std::chrono::seconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(pollInterval);
    }
    return condition();
}

struct ReceivedMessages {
    std::mutex mutex;
    std::vector<std::string> messages;

    [[nodiscard]] bool contains(const std::string& message) {
        std::scoped_lock lock(this->mutex);
        return std::ranges::find(this->messages, message) !=
            this->messages.end();
    }
};

} // namespace

TEST(StreamrNodeCppWrapperTest, InvalidEthereumAddressThrows) {
    try {
        StreamrNode node("not-an-address");
        FAIL() << "expected StreamrNodeError";
    } catch (const StreamrNodeError& error) {
        EXPECT_TRUE(
            std::holds_alternative<ErrorInvalidEthereumAddress>(error.code));
    }
}

TEST(StreamrNodeCppWrapperTest, TwoNodesExchangeMessages) {
    // Node A runs the websocket server of a new network; node B joins
    // through it.
    StreamrNode nodeA(
        ethereumAddressA, StreamrNodeOptions{.websocketPort = entryPointPort});
    nodeA.start();

    StreamrNode nodeB(
        ethereumAddressB,
        StreamrNodeOptions{
            .entryPoints = {StreamrProxyAddress{
                .websocketUrl =
                    "ws://127.0.0.1:" + std::to_string(entryPointPort),
                .ethereumAddress = ethereumAddressA}}});
    nodeB.start();

    ReceivedMessages receivedA;
    ReceivedMessages receivedB;
    const auto subscriptionA = nodeA.subscribe(
        streamPartId,
        [&receivedA](
            std::string_view /*streamPartId*/, std::string_view content) {
            std::scoped_lock lock(receivedA.mutex);
            receivedA.messages.emplace_back(content);
        });
    const auto subscriptionB = nodeB.subscribe(
        streamPartId,
        [&receivedB](
            std::string_view /*streamPartId*/, std::string_view content) {
            std::scoped_lock lock(receivedB.mutex);
            receivedB.messages.emplace_back(content);
        });

    ASSERT_TRUE(waitUntil(
        [&]() {
            return nodeA.neighborCount(streamPartId) >= 1 &&
                nodeB.neighborCount(streamPartId) >= 1;
        },
        topologyTimeout));

    const std::string messageFromA = "hello from A (signed)";
    nodeA.publish(streamPartId, messageFromA, ethereumPrivateKey);
    EXPECT_TRUE(waitUntil(
        [&]() { return receivedB.contains(messageFromA); }, messageTimeout));

    const std::string messageFromB = "hello from B (unsigned)";
    nodeB.publish(streamPartId, messageFromB);
    EXPECT_TRUE(waitUntil(
        [&]() { return receivedA.contains(messageFromB); }, messageTimeout));

    nodeB.unsubscribe(subscriptionB);
    try {
        nodeB.unsubscribe(subscriptionB);
        FAIL() << "expected StreamrNodeError";
    } catch (const StreamrNodeError& error) {
        EXPECT_TRUE(
            std::holds_alternative<ErrorSubscriptionNotFound>(error.code));
    }
    nodeA.unsubscribe(subscriptionA);

    nodeB.stop();
    nodeA.stop();
}
