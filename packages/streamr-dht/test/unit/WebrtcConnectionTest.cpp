// Ported from packages/dht/test/unit/WebrtcConnection.test.ts
// (v103.8.0-rc.3): a started connection whose remote descriptor is never
// set closes itself after EARLY_TIMEOUT with the ported reason string.
// Adaptation: jest's waitForEvent becomes a Disconnected listener captured
// into flags polled with waitForCondition.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.dht.Connection;
import streamr.dht.TestUtils;
import streamr.dht.WebrtcConnection;
import streamr.dht.webrtcTypes;
import streamr.utils.CoroutineHelper;
import streamr.utils.waitForCondition;

using streamr::dht::connection::connectionevents::Disconnected;
using streamr::dht::connection::webrtc::WebrtcConnection;
using streamr::dht::connection::webrtc::WebrtcConnectionParams;
using streamr::dht::testutils::createMockPeerDescriptor;
using streamr::utils::blockingWait;
using streamr::utils::waitForCondition;

namespace {
// TS waits 5001 ms for the 5000 ms EARLY_TIMEOUT; polling needs a little
// more headroom.
constexpr auto disconnectTimeout = std::chrono::milliseconds(7000);
constexpr auto pollInterval = std::chrono::milliseconds(100);
} // namespace

TEST(WebrtcConnectionTest, DisconnectsEarlyIfRemoteDescriptorIsNotSet) {
    auto connection = WebrtcConnection::newInstance(
        WebrtcConnectionParams{
            .remotePeerDescriptor = createMockPeerDescriptor()});

    std::atomic<bool> disconnected = false;
    std::mutex reasonMutex;
    std::string reason;
    connection->on<Disconnected>([&](bool /*gracefulLeave*/,
                                     uint64_t /*code*/,
                                     const std::string& disconnectionReason) {
        {
            std::scoped_lock lock(reasonMutex);
            reason = disconnectionReason;
        }
        disconnected = true;
    });

    connection->start(true);

    blockingWait(waitForCondition(
        [&disconnected]() { return disconnected.load(); },
        disconnectTimeout,
        pollInterval));
    std::scoped_lock lock(reasonMutex);
    EXPECT_EQ(reason, "timed out due to remote descriptor not being set");

    connection->close(true);
}
