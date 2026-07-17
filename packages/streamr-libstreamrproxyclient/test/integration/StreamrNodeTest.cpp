// Tests of the streamrNode* C API (streamrnode.h): input validation and
// lifecycle errors, a two-node publish/subscribe exchange over real
// websockets on 127.0.0.1, and the proxy mode folded into the node
// handle (a client-only node proxy-publishing into a full node that
// accepts proxy connections).
#include "streamrnode.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <gtest/gtest.h>

namespace {

// The C API is synchronous but topology formation is not; poll until
// the condition holds or the deadline passes.
constexpr auto pollInterval = std::chrono::milliseconds(200);

bool waitUntil(
    const std::function<bool()>& condition, std::chrono::seconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (condition()) {
            return true;
        }
        std::this_thread::sleep_for(pollInterval);
    }
    return condition();
}

// Collects the contents delivered to a subscription callback. The
// callback runs on an internal network thread, hence the mutex.
struct ReceivedMessages {
    std::mutex mutex;
    std::vector<std::string> contents;

    static void callback(
        uint64_t /* nodeHandle */,
        const char* /* streamPartId */,
        const char* content,
        uint64_t contentLength,
        void* userData) {
        auto* self = static_cast<ReceivedMessages*>(userData);
        const std::scoped_lock lock(self->mutex);
        self->contents.emplace_back(content, contentLength);
    }

    bool contains(const std::string& expected) {
        const std::scoped_lock lock(this->mutex);
        return std::ranges::any_of(
            this->contents,
            [&expected](const auto& content) { return content == expected; });
    }
};

} // namespace

class StreamrNodeTest : public ::testing::Test {
protected:
    static constexpr const char* invalidEthereumAddress =
        "INVALID_ETHEREUM_ADDRESS";
    static constexpr const char* ethereumAddressA =
        "0x1234567890123456789012345678901234567890";
    static constexpr const char* ethereumAddressB =
        "0x1234567890123456789012345678901234567892";
    // Any valid secp256k1 scalar works as a signing key; the receivers
    // do not verify signatures.
    static constexpr const char* ethereumPrivateKey =
        "1111111111111111111111111111111111111111111111111111111111111111";
    static constexpr const char* invalidStreamPartId = "INVALID_STREAM_PART_ID";
    static constexpr const char* validStreamPartId =
        "0xa000000000000000000000000000000000000000#01";
    static constexpr const char* invalidUrl = "poiejrg039utg240";
    static constexpr uint64_t nonExistentNodeHandle = 12345;
    static constexpr uint16_t exchangeEntryPointPort = 44451;
    static constexpr uint16_t proxyServerPort = 44452;
    static constexpr auto topologyTimeout = std::chrono::seconds(60);
    static constexpr auto messageTimeout = std::chrono::seconds(30);

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    static void expectSingleError(
        const StreamrResult* result, const char* expectedCode) {
        ASSERT_NE(result, nullptr);
        ASSERT_EQ(result->numErrors, 1);
        EXPECT_STREQ(result->errors[0].code, expectedCode);
    }

    static void expectNoErrors(const StreamrResult* result) {
        ASSERT_NE(result, nullptr);
        EXPECT_EQ(result->numErrors, 0);
    }

public:
    // The fixture destructor of the sibling proxy-client test cleans the
    // library up after every test; re-initialize here so consecutive
    // tests also work when the binary is run as a single process (under
    // ctest each test is its own process and this is a no-op).
    StreamrNodeTest() { streamrInitLibrary(); }
    ~StreamrNodeTest() override { streamrCleanupLibrary(); }
};

TEST_F(StreamrNodeTest, InvalidEthereumAddress) {
    const StreamrResult* result = nullptr;
    uint64_t nodeHandle =
        streamrNodeNew(&result, invalidEthereumAddress, nullptr);
    EXPECT_EQ(nodeHandle, 0);
    expectSingleError(result, ERROR_INVALID_ETHEREUM_ADDRESS);
    streamrResultDelete(result);
}

TEST_F(StreamrNodeTest, InvalidEntryPointUrl) {
    const StreamrResult* result = nullptr;
    StreamrEntryPoint entryPoint{
        .websocketUrl = invalidUrl, .ethereumAddress = ethereumAddressB};
    StreamrNodeConfig config{.entryPoints = &entryPoint, .numEntryPoints = 1};
    uint64_t nodeHandle = streamrNodeNew(&result, ethereumAddressA, &config);
    EXPECT_EQ(nodeHandle, 0);
    expectSingleError(result, ERROR_INVALID_ENTRY_POINT_URL);
    streamrResultDelete(result);
}

TEST_F(StreamrNodeTest, InvalidEntryPointEthereumAddress) {
    const StreamrResult* result = nullptr;
    StreamrEntryPoint entryPoint{
        .websocketUrl = "ws://127.0.0.1:40000",
        .ethereumAddress = invalidEthereumAddress};
    StreamrNodeConfig config{.entryPoints = &entryPoint, .numEntryPoints = 1};
    uint64_t nodeHandle = streamrNodeNew(&result, ethereumAddressA, &config);
    EXPECT_EQ(nodeHandle, 0);
    expectSingleError(result, ERROR_INVALID_ETHEREUM_ADDRESS);
    streamrResultDelete(result);
}

TEST_F(StreamrNodeTest, NodeNotFound) {
    const StreamrResult* result = nullptr;

    streamrNodeStart(&result, nonExistentNodeHandle);
    expectSingleError(result, ERROR_NODE_NOT_FOUND);
    streamrResultDelete(result);

    streamrNodeStop(&result, nonExistentNodeHandle);
    expectSingleError(result, ERROR_NODE_NOT_FOUND);
    streamrResultDelete(result);

    streamrNodeJoinStreamPart(
        &result, nonExistentNodeHandle, validStreamPartId, nullptr, 0);
    expectSingleError(result, ERROR_NODE_NOT_FOUND);
    streamrResultDelete(result);

    streamrNodeLeaveStreamPart(
        &result, nonExistentNodeHandle, validStreamPartId);
    expectSingleError(result, ERROR_NODE_NOT_FOUND);
    streamrResultDelete(result);

    streamrNodePublish(
        &result, nonExistentNodeHandle, validStreamPartId, "x", 1, nullptr);
    expectSingleError(result, ERROR_NODE_NOT_FOUND);
    streamrResultDelete(result);

    uint64_t subscriptionHandle = streamrNodeSubscribe(
        &result,
        nonExistentNodeHandle,
        validStreamPartId,
        ReceivedMessages::callback,
        nullptr);
    EXPECT_EQ(subscriptionHandle, 0);
    expectSingleError(result, ERROR_NODE_NOT_FOUND);
    streamrResultDelete(result);

    streamrNodeUnsubscribe(&result, nonExistentNodeHandle, 1);
    expectSingleError(result, ERROR_NODE_NOT_FOUND);
    streamrResultDelete(result);

    streamrNodeGetNeighborCount(
        &result, nonExistentNodeHandle, validStreamPartId);
    expectSingleError(result, ERROR_NODE_NOT_FOUND);
    streamrResultDelete(result);

    StreamrPeer proxy{
        .websocketUrl = "ws://127.0.0.1:40000",
        .ethereumAddress = ethereumAddressB};
    streamrNodeSetProxies(
        &result,
        nonExistentNodeHandle,
        validStreamPartId,
        &proxy,
        1,
        STREAMR_PROXY_DIRECTION_PUBLISH,
        1);
    expectSingleError(result, ERROR_NODE_NOT_FOUND);
    streamrResultDelete(result);
}

TEST_F(StreamrNodeTest, CanCreateAndDeleteWithoutStarting) {
    const StreamrResult* result = nullptr;
    uint64_t nodeHandle = streamrNodeNew(&result, ethereumAddressA, nullptr);
    EXPECT_NE(nodeHandle, 0);
    expectNoErrors(result);
    streamrResultDelete(result);

    const StreamrResult* result2 = nullptr;
    streamrNodeDelete(&result2, nodeHandle);
    expectNoErrors(result2);
    streamrResultDelete(result2);
}

TEST_F(StreamrNodeTest, OperationsRequireStart) {
    const StreamrResult* result = nullptr;
    uint64_t nodeHandle = streamrNodeNew(&result, ethereumAddressA, nullptr);
    ASSERT_NE(nodeHandle, 0);
    streamrResultDelete(result);

    streamrNodeJoinStreamPart(
        &result, nodeHandle, validStreamPartId, nullptr, 0);
    expectSingleError(result, ERROR_NODE_NOT_STARTED);
    streamrResultDelete(result);

    streamrNodePublish(&result, nodeHandle, validStreamPartId, "x", 1, nullptr);
    expectSingleError(result, ERROR_NODE_NOT_STARTED);
    streamrResultDelete(result);

    uint64_t subscriptionHandle = streamrNodeSubscribe(
        &result,
        nodeHandle,
        validStreamPartId,
        ReceivedMessages::callback,
        nullptr);
    EXPECT_EQ(subscriptionHandle, 0);
    expectSingleError(result, ERROR_NODE_NOT_STARTED);
    streamrResultDelete(result);

    streamrNodeGetNeighborCount(&result, nodeHandle, validStreamPartId);
    expectSingleError(result, ERROR_NODE_NOT_STARTED);
    streamrResultDelete(result);

    streamrNodeStop(&result, nodeHandle);
    expectSingleError(result, ERROR_NODE_NOT_STARTED);
    streamrResultDelete(result);

    streamrNodeUnsubscribe(&result, nodeHandle, 1);
    expectSingleError(result, ERROR_SUBSCRIPTION_NOT_FOUND);
    streamrResultDelete(result);

    streamrNodeDelete(&result, nodeHandle);
    expectNoErrors(result);
    streamrResultDelete(result);
}

TEST_F(StreamrNodeTest, StartStopLifecycle) {
    // No entry points and no websocket server: the node starts an
    // isolated network of its own (it is its own entry point).
    const StreamrResult* result = nullptr;
    uint64_t nodeHandle = streamrNodeNew(&result, ethereumAddressA, nullptr);
    ASSERT_NE(nodeHandle, 0);
    streamrResultDelete(result);

    streamrNodeStart(&result, nodeHandle);
    expectNoErrors(result);
    streamrResultDelete(result);

    streamrNodeStart(&result, nodeHandle);
    expectSingleError(result, ERROR_NODE_ALREADY_STARTED);
    streamrResultDelete(result);

    // The stream-part-id validation needs a running node because the
    // running-state check comes first.
    streamrNodeJoinStreamPart(
        &result, nodeHandle, invalidStreamPartId, nullptr, 0);
    expectSingleError(result, ERROR_INVALID_STREAM_PART_ID);
    streamrResultDelete(result);

    streamrNodeStop(&result, nodeHandle);
    expectNoErrors(result);
    streamrResultDelete(result);

    // Stopping is idempotent.
    streamrNodeStop(&result, nodeHandle);
    expectNoErrors(result);
    streamrResultDelete(result);

    // A stopped node cannot be restarted or used.
    streamrNodeStart(&result, nodeHandle);
    expectSingleError(result, ERROR_NODE_STOPPED);
    streamrResultDelete(result);

    streamrNodePublish(&result, nodeHandle, validStreamPartId, "x", 1, nullptr);
    expectSingleError(result, ERROR_NODE_STOPPED);
    streamrResultDelete(result);

    streamrNodeDelete(&result, nodeHandle);
    expectNoErrors(result);
    streamrResultDelete(result);
}

TEST_F(StreamrNodeTest, TwoNodesExchangeMessages) {
    // Node A runs a websocket server and acts as the entry point of a
    // new network; node B joins through it as a websocket client.
    const StreamrResult* result = nullptr;
    StreamrNodeConfig configA{.websocketPort = exchangeEntryPointPort};
    uint64_t nodeA = streamrNodeNew(&result, ethereumAddressA, &configA);
    ASSERT_NE(nodeA, 0);
    streamrResultDelete(result);
    streamrNodeStart(&result, nodeA);
    expectNoErrors(result);
    streamrResultDelete(result);

    const std::string entryPointUrl =
        "ws://127.0.0.1:" + std::to_string(exchangeEntryPointPort);
    StreamrEntryPoint entryPoint{
        .websocketUrl = entryPointUrl.c_str(),
        .ethereumAddress = ethereumAddressA};
    StreamrNodeConfig configB{.entryPoints = &entryPoint, .numEntryPoints = 1};
    uint64_t nodeB = streamrNodeNew(&result, ethereumAddressB, &configB);
    ASSERT_NE(nodeB, 0);
    streamrResultDelete(result);
    streamrNodeStart(&result, nodeB);
    expectNoErrors(result);
    streamrResultDelete(result);

    // Both nodes subscribe (which also joins the stream part).
    ReceivedMessages receivedA;
    ReceivedMessages receivedB;
    uint64_t subscriptionA = streamrNodeSubscribe(
        &result,
        nodeA,
        validStreamPartId,
        ReceivedMessages::callback,
        &receivedA);
    ASSERT_NE(subscriptionA, 0);
    streamrResultDelete(result);
    uint64_t subscriptionB = streamrNodeSubscribe(
        &result,
        nodeB,
        validStreamPartId,
        ReceivedMessages::callback,
        &receivedB);
    ASSERT_NE(subscriptionB, 0);
    streamrResultDelete(result);

    // Wait for the stream part topology to form between the two nodes.
    EXPECT_TRUE(waitUntil(
        [&]() {
            const StreamrResult* pollResult = nullptr;
            auto neighborsA = streamrNodeGetNeighborCount(
                &pollResult, nodeA, validStreamPartId);
            streamrResultDelete(pollResult);
            auto neighborsB = streamrNodeGetNeighborCount(
                &pollResult, nodeB, validStreamPartId);
            streamrResultDelete(pollResult);
            return neighborsA >= 1 && neighborsB >= 1;
        },
        topologyTimeout));

    // A signed message from A to B, an unsigned one from B to A.
    const std::string messageFromA = "hello from A";
    streamrNodePublish(
        &result,
        nodeA,
        validStreamPartId,
        messageFromA.data(),
        messageFromA.size(),
        ethereumPrivateKey);
    expectNoErrors(result);
    streamrResultDelete(result);
    EXPECT_TRUE(waitUntil(
        [&]() { return receivedB.contains(messageFromA); }, messageTimeout));

    const std::string messageFromB = "hello from B";
    streamrNodePublish(
        &result,
        nodeB,
        validStreamPartId,
        messageFromB.data(),
        messageFromB.size(),
        nullptr);
    expectNoErrors(result);
    streamrResultDelete(result);
    EXPECT_TRUE(waitUntil(
        [&]() { return receivedA.contains(messageFromB); }, messageTimeout));

    streamrNodeUnsubscribe(&result, nodeB, subscriptionB);
    expectNoErrors(result);
    streamrResultDelete(result);
    streamrNodeUnsubscribe(&result, nodeB, subscriptionB);
    expectSingleError(result, ERROR_SUBSCRIPTION_NOT_FOUND);
    streamrResultDelete(result);
    streamrNodeUnsubscribe(&result, nodeA, subscriptionA);
    expectNoErrors(result);
    streamrResultDelete(result);

    streamrNodeStop(&result, nodeB);
    expectNoErrors(result);
    streamrResultDelete(result);
    streamrNodeStop(&result, nodeA);
    expectNoErrors(result);
    streamrResultDelete(result);
    streamrNodeDelete(&result, nodeB);
    streamrResultDelete(result);
    streamrNodeDelete(&result, nodeA);
    streamrResultDelete(result);
}

TEST_F(StreamrNodeTest, ProxyModePublishesToAcceptingNode) {
    // Node A is a full node accepting proxy connections; node B is a
    // client-only node (own isolated DHT, no websocket server) that
    // proxy-publishes into A — the old proxy-client use case through
    // the node handle.
    const StreamrResult* result = nullptr;
    StreamrNodeConfig configA{
        .websocketPort = proxyServerPort, .acceptProxyConnections = true};
    uint64_t nodeA = streamrNodeNew(&result, ethereumAddressA, &configA);
    ASSERT_NE(nodeA, 0);
    streamrResultDelete(result);
    streamrNodeStart(&result, nodeA);
    expectNoErrors(result);
    streamrResultDelete(result);

    ReceivedMessages receivedA;
    uint64_t subscriptionA = streamrNodeSubscribe(
        &result,
        nodeA,
        validStreamPartId,
        ReceivedMessages::callback,
        &receivedA);
    ASSERT_NE(subscriptionA, 0);
    streamrResultDelete(result);

    // A node that accepts proxy connections cannot itself use proxies.
    const std::string proxyUrl =
        "ws://127.0.0.1:" + std::to_string(proxyServerPort);
    StreamrPeer proxy{
        .websocketUrl = proxyUrl.c_str(), .ethereumAddress = ethereumAddressA};
    streamrNodeSetProxies(
        &result,
        nodeA,
        validStreamPartId,
        &proxy,
        1,
        STREAMR_PROXY_DIRECTION_PUBLISH,
        1);
    expectSingleError(result, ERROR_NODE_OPERATION_FAILED);
    streamrResultDelete(result);

    uint64_t nodeB = streamrNodeNew(&result, ethereumAddressB, nullptr);
    ASSERT_NE(nodeB, 0);
    streamrResultDelete(result);
    streamrNodeStart(&result, nodeB);
    expectNoErrors(result);
    streamrResultDelete(result);

    streamrNodeSetProxies(
        &result,
        nodeB,
        validStreamPartId,
        &proxy,
        1,
        STREAMR_PROXY_DIRECTION_PUBLISH,
        1);
    expectNoErrors(result);
    streamrResultDelete(result);

    const std::string messageViaProxy = "hello via proxy";
    EXPECT_TRUE(waitUntil(
        [&]() {
            const StreamrResult* publishResult = nullptr;
            streamrNodePublish(
                &publishResult,
                nodeB,
                validStreamPartId,
                messageViaProxy.data(),
                messageViaProxy.size(),
                nullptr);
            streamrResultDelete(publishResult);
            return receivedA.contains(messageViaProxy);
        },
        messageTimeout));

    streamrNodeStop(&result, nodeB);
    streamrResultDelete(result);
    streamrNodeStop(&result, nodeA);
    streamrResultDelete(result);
    streamrNodeDelete(&result, nodeB);
    streamrResultDelete(result);
    streamrNodeDelete(&result, nodeA);
    streamrResultDelete(result);
}
