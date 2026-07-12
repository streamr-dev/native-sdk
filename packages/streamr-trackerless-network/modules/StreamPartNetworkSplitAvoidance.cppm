// Module streamr.trackerlessnetwork.StreamPartNetworkSplitAvoidance
// Ported from packages/trackerless-network/src/
// StreamPartNetworkSplitAvoidance.ts (v103.8.0-rc.3): tries to find new
// neighbors when the node has fewer than MIN_NEIGHBOR_COUNT by rejoining
// the stream's control-layer network, avoiding some network-split
// scenarios (most relevant for small stream networks).
//
// Port notes: TS exponentialRunOff runs ALL maxAttempts even after the
// task stops throwing (no early exit) — the unit test pins that behavior
// (it expects the neighbor count to keep growing past the threshold), so
// the port preserves it. TS's discoverEntryPoints option declares an
// optional excluded-nodes parameter, but the class never passes one (it
// filters locally) — the port takes a parameterless callback.
module;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <coroutine> // IWYU pragma: keep
#include <ranges>

export module streamr.trackerlessnetwork.StreamPartNetworkSplitAvoidance;

import streamr.dht.protos;

import streamr.dht.Identifiers;
import streamr.logger.SLogger;
import streamr.trackerlessnetwork.DiscoveryLayerNode;
import streamr.utils.AbortController;
import streamr.utils.CoroutineHelper;

// Hoisted from the former-header idiom (file scope, NOT exported).
using streamr::logger::SLogger;

namespace {

// TS exponentialRunOff: run `task` maxAttempts times with exponentially
// growing abortable waits between attempts; task failures are swallowed
// (the next attempt retries), and an abort ends the loop at the next
// checkpoint.
folly::coro::Task<void> exponentialRunOff(
    std::function<folly::coro::Task<void>()> task,
    std::string description,
    streamr::utils::AbortSignal& abortSignal,
    std::chrono::milliseconds baseDelay = std::chrono::milliseconds{500},
    size_t maxAttempts = 6) { // NOLINT(readability-magic-numbers)
    const auto cancellationToken = abortSignal.getCancellationToken();
    for (size_t i = 1; i <= maxAttempts; i++) {
        if (abortSignal.aborted) {
            co_return;
        }
        const auto factor = static_cast<int64_t>(1) << i;
        const auto delay = baseDelay * factor;
        try {
            co_await task();
        } catch (const std::exception&) {
            SLogger::trace(
                description + " failed, retrying in " +
                std::to_string(delay.count()) + " ms");
        }
        try {
            co_await streamr::utils::co_withCancellation(
                cancellationToken, folly::coro::sleep(delay));
        } catch (const std::exception& err) {
            SLogger::trace(err.what());
        }
    }
}

} // namespace

export namespace streamr::trackerlessnetwork {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::trackerlessnetwork::discoverylayer::DiscoveryLayerNode;
using streamr::utils::AbortController;

constexpr size_t minNeighborCount = 4; // TS MIN_NEIGHBOR_COUNT

struct StreamPartNetworkSplitAvoidanceOptions {
    DiscoveryLayerNode& discoveryLayerNode;
    std::function<folly::coro::Task<std::vector<PeerDescriptor>>()>
        discoverEntryPoints;
    std::optional<std::chrono::milliseconds> exponentialRunOfBaseDelay;
};

class StreamPartNetworkSplitAvoidance {
private:
    static constexpr std::chrono::milliseconds defaultRunOffBaseDelay{500};

    StreamPartNetworkSplitAvoidanceOptions options;
    AbortController abortController;
    std::set<DhtAddress> excludedNodes;
    // Read by isRunning() from other threads while avoidNetworkSplit() is
    // in flight on a pool thread.
    std::atomic<bool> running = false;

public:
    explicit StreamPartNetworkSplitAvoidance(
        StreamPartNetworkSplitAvoidanceOptions options)
        : options(std::move(options)) {}

    folly::coro::Task<void> avoidNetworkSplit() {
        this->running = true;
        co_await exponentialRunOff(
            [this]() -> folly::coro::Task<void> {
                const auto discoveredEntryPoints =
                    co_await this->options.discoverEntryPoints();
                std::vector<PeerDescriptor> filteredEntryPoints;
                for (const auto& peer : discoveredEntryPoints) {
                    if (!this->excludedNodes.contains(
                            Identifiers::getNodeIdFromPeerDescriptor(peer))) {
                        filteredEntryPoints.push_back(peer);
                    }
                }
                co_await this->options.discoveryLayerNode.joinDht(
                    filteredEntryPoints,
                    /*doRandomJoin=*/false,
                    /*retry=*/false);
                if (this->options.discoveryLayerNode.getNeighborCount() <
                    minNeighborCount) {
                    // Exclude entry points that did not become neighbors
                    // (assumed offline).
                    const auto neighbors =
                        this->options.discoveryLayerNode.getNeighbors();
                    for (const auto& peer : filteredEntryPoints) {
                        const auto isNeighbor =
                            [&peer](const PeerDescriptor& neighbor) {
                                return Identifiers::areEqualPeerDescriptors(
                                    neighbor, peer);
                            };
                        if (!std::ranges::any_of(neighbors, isNeighbor)) {
                            this->excludedNodes.insert(
                                Identifiers::getNodeIdFromPeerDescriptor(peer));
                        }
                    }
                    throw std::runtime_error("Network split is still possible");
                }
            },
            "avoid network split",
            this->abortController.getSignal(),
            this->options.exponentialRunOfBaseDelay.value_or(
                defaultRunOffBaseDelay));
        this->running = false;
        this->excludedNodes.clear();
        SLogger::trace("Network split avoided");
    }

    [[nodiscard]] bool isRunning() const { return this->running; }

    void destroy() { this->abortController.abort(); }
};

} // namespace streamr::trackerlessnetwork
