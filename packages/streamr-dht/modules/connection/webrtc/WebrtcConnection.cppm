// Module streamr.dht.WebrtcConnection
// Ported from packages/dht/src/nodejs/WebrtcConnection.ts (v103.8.0-rc.3):
// one WebRTC data-channel connection over libdatachannel's
// rtc::PeerConnection/rtc::DataChannel — the TS node-datachannel binding
// wraps the exact same library, so the callback semantics match closely.
// Adaptations from the TS original:
//  - the localDescription/localCandidate events live on a separate member
//    emitter (webrtcEvents()) because the C++ Connection base fixes its
//    event tuple (see streamr.dht.webrtcTypes);
//  - the EARLY_TIMEOUT setTimeout becomes an AbortableTimers timeout with a
//    WEAK self (the PendingConnection watchdog pattern), scheduled by
//    newInstance after make_shared;
//  - state is guarded by mMutex (rtc callbacks fire on libdatachannel's
//    processor threads), following the WebsocketConnection idiom, including
//    closing the rtc objects OUTSIDE the mutex (rtc holds its callback
//    mutex while callbacks run and our callbacks take mMutex — calling into
//    rtc under mMutex is the lock-order inversion that deadlocked the
//    websocket teardowns).
module;

#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <rtc/rtc.hpp>

export module streamr.dht.WebrtcConnection;

import streamr.dht.protos;

import streamr.dht.Connection;
import streamr.dht.Identifiers;
import streamr.dht.webrtcTypes;
import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;
import streamr.utils.AbortableTimers;
import streamr.utils.AbortController;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.SharedExecutors;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;
using streamr::utils::AbortableTimers;
using streamr::utils::AbortController;
using streamr::utils::EnableSharedFromThis;

export namespace streamr::dht::connection::webrtc {

using streamr::dht::Identifiers;
using streamr::dht::connection::Connection;
using streamr::dht::connection::ConnectionType;
using streamr::dht::connection::connectionevents::Connected;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::connection::connectionevents::Disconnected;

inline constexpr size_t defaultBufferThresholdHigh = 1U << 17U;
inline constexpr size_t defaultBufferThresholdLow = 1U << 15U;
inline constexpr size_t defaultMaxMessageSize = 1048576;

// TS RtcPeerConnectionStateEnum — libdatachannel reports the same states
// as a typed enum, so the TS unknown-state guard is unrepresentable here.
class WebrtcConnection : public Connection, public EnableSharedFromThis {
private:
    WebrtcConnectionParams params;
    std::shared_ptr<rtc::PeerConnection> connection;
    std::shared_ptr<rtc::DataChannel> dataChannel;
    rtc::PeerConnection::State lastState =
        rtc::PeerConnection::State::Connecting;
    bool remoteDescriptionSet = false;
    // Candidates received before the remote description is set (flushed by
    // setRemoteDescription). {candidate, mid} pairs.
    std::vector<std::pair<std::string, std::string>> pendingCandidates;
    bool closed = false;
    std::optional<bool> offering;
    std::deque<std::vector<std::byte>> messageQueue;
    AbortController earlyTimeoutAbort;
    streamr::eventemitter::EventEmitter<WebrtcSignallingEvents>
        signallingEvents;
    std::recursive_mutex mMutex;

    explicit WebrtcConnection(WebrtcConnectionParams&& params)
        : Connection(ConnectionType::WEBRTC), params(std::move(params)) {}

    // Called by newInstance right after make_shared (sharedFromThis is not
    // available in the constructor). Weak self: the timeout must not keep a
    // torn-down connection alive, and a fired timeout on a dead connection
    // is a no-op.
    void scheduleEarlyTimeout() {
        std::weak_ptr<WebrtcConnection> weakSelf =
            this->sharedFromThis<WebrtcConnection>();
        AbortableTimers::setAbortableTimeout(
            [weakSelf]() {
                if (auto self = weakSelf.lock()) {
                    self->doClose(
                        false,
                        "timed out due to remote descriptor not being set");
                }
            },
            earlyTimeout,
            this->earlyTimeoutAbort.getSignal());
    }

public:
    [[nodiscard]] static std::shared_ptr<WebrtcConnection> newInstance(
        WebrtcConnectionParams params) {
        struct MakeSharedEnabler : public WebrtcConnection {
            explicit MakeSharedEnabler(WebrtcConnectionParams&& params)
                : WebrtcConnection(std::move(params)) {}
        };
        auto instance = std::make_shared<MakeSharedEnabler>(std::move(params));
        instance->scheduleEarlyTimeout();
        return instance;
    }

    ~WebrtcConnection() override { destroy(); }

    // The localDescription/localCandidate signalling events (TS
    // WebrtcConnectionEvents extras).
    [[nodiscard]] streamr::eventemitter::EventEmitter<WebrtcSignallingEvents>&
    webrtcEvents() {
        return this->signallingEvents;
    }

    void start(bool isOffering) {
        const auto nodeId = Identifiers::getNodeIdFromPeerDescriptor(
            this->params.remotePeerDescriptor);
        SLogger::trace(
            "Starting new connection for peer " + nodeId,
            {{"isOffering", isOffering}});
        auto self = this->sharedFromThis<WebrtcConnection>();
        std::scoped_lock lock(this->mMutex);
        this->offering = isOffering;

        rtc::Configuration configuration;
        for (const auto& iceServer : this->params.iceServers) {
            configuration.iceServers.emplace_back(iceServerAsString(iceServer));
        }
        configuration.maxMessageSize =
            this->params.maxMessageSize.value_or(defaultMaxMessageSize);
        if (this->params.portRange.has_value()) {
            configuration.portRangeBegin = this->params.portRange->min;
            configuration.portRangeEnd = this->params.portRange->max;
        }
        this->connection = std::make_shared<rtc::PeerConnection>(configuration);

        this->connection->onStateChange(
            [self](rtc::PeerConnection::State state) {
                self->onStateChange(state);
            });
        this->connection->onGatheringStateChange(
            [](rtc::PeerConnection::GatheringState) {});
        this->connection->onLocalDescription(
            [self](const rtc::Description& description) {
                self->signallingEvents
                    .emit<webrtcconnectionevents::LocalDescription>(
                        std::string(description), description.typeString());
            });
        this->connection->onLocalCandidate(
            [self](const rtc::Candidate& candidate) {
                self->signallingEvents
                    .emit<webrtcconnectionevents::LocalCandidate>(
                        candidate.candidate(), candidate.mid());
            });
        if (isOffering) {
            this->setupDataChannel(
                this->connection->createDataChannel("streamrDataChannel"));
        } else {
            this->connection->onDataChannel(
                [self](std::shared_ptr<rtc::DataChannel> dataChannel) {
                    std::scoped_lock lock(self->mMutex);
                    self->setupDataChannel(std::move(dataChannel));
                });
        }
    }

    void setRemoteDescription(
        const std::string& description, const std::string& type) {
        std::scoped_lock lock(this->mMutex);
        if (this->connection) {
            this->earlyTimeoutAbort.abort();
            const auto remoteNodeId = Identifiers::getNodeIdFromPeerDescriptor(
                this->params.remotePeerDescriptor);
            try {
                SLogger::trace(
                    "Setting remote descriptor for peer: " + remoteNodeId);
                this->connection->setRemoteDescription(
                    rtc::Description(description, type));
                this->remoteDescriptionSet = true;
                // Apply any candidates that arrived before the description
                // (see addRemoteCandidate).
                for (const auto& pending : this->pendingCandidates) {
                    try {
                        this->connection->addRemoteCandidate(
                            rtc::Candidate(pending.first, pending.second));
                    } catch (const std::exception&) {
                        SLogger::debug(
                            "Failed to set queued remote candidate for peer " +
                            remoteNodeId);
                    }
                }
                this->pendingCandidates.clear();
            } catch (const std::exception&) {
                SLogger::debug(
                    "Failed to set remote descriptor for peer " + remoteNodeId);
            }
        } else {
            this->doClose(
                false, "Tried to set description for non-existent connection");
        }
    }

    void addRemoteCandidate(
        const std::string& candidate, const std::string& mid) {
        std::scoped_lock lock(this->mMutex);
        if (this->connection) {
            if (this->remoteDescriptionSet) {
                const auto remoteNodeId =
                    Identifiers::getNodeIdFromPeerDescriptor(
                        this->params.remotePeerDescriptor);
                try {
                    SLogger::trace(
                        "Setting remote candidate for peer: " + remoteNodeId);
                    this->connection->addRemoteCandidate(
                        rtc::Candidate(candidate, mid));
                } catch (const std::exception&) {
                    SLogger::debug(
                        "Failed to set remote candidate for peer " +
                        remoteNodeId);
                }
            } else {
                // Queue the candidate until the remote description is set
                // (implements the TS TODO at this exact spot). Over the
                // simulator the offerer's separate rtcOffer and iceCandidate
                // notifications can reach the answerer out of order, so a
                // candidate legitimately arrives before the offer is applied
                // — TS closes here, which is the documented flaky test
                // NET-911; queuing removes the flakiness without any
                // wire-visible change (setRemoteDescription flushes these).
                SLogger::trace(
                    "Queuing remote candidate until description is set");
                this->pendingCandidates.emplace_back(candidate, mid);
            }
        } else {
            this->doClose(
                false, "Tried to set candidate for non-existent connection");
        }
    }

    void send(const std::vector<std::byte>& data) override {
        std::scoped_lock lock(this->mMutex);
        if (this->isOpen()) {
            try {
                if (this->dataChannel->bufferedAmount() <
                    this->params.bufferThresholdHigh.value_or(
                        defaultBufferThresholdHigh)) {
                    this->dataChannel->send(data.data(), data.size());
                } else {
                    this->messageQueue.push_back(data);
                }
            } catch (const std::exception& err) {
                SLogger::debug(
                    "Failed to send binary message to " +
                    Identifiers::getNodeIdFromPeerDescriptor(
                        this->params.remotePeerDescriptor) +
                    " " + err.what());
            }
        }
    }

    void close(bool gracefulLeave) override { this->doClose(gracefulLeave); }

    void destroy() override {
        this->removeAllListeners();
        this->signallingEvents.removeAllListeners();
        this->doClose(false);
    }

    [[nodiscard]] bool isOpen() {
        std::scoped_lock lock(this->mMutex);
        return !this->closed &&
            this->lastState == rtc::PeerConnection::State::Connected &&
            this->dataChannel != nullptr;
    }

private:
    void doClose(bool gracefulLeave, const std::string& reason = "") {
        std::shared_ptr<rtc::DataChannel> dataChannelToClose;
        std::shared_ptr<rtc::PeerConnection> connectionToClose;
        {
            std::scoped_lock lock(this->mMutex);
            if (this->closed) {
                return;
            }
            this->earlyTimeoutAbort.abort();
            SLogger::trace(
                "Closing Node WebRTC Connection to " +
                    Identifiers::getNodeIdFromPeerDescriptor(
                        this->params.remotePeerDescriptor),
                {{"reason", reason}});
            // Setting closed=true under the lock with the early-return guard
            // makes exactly one doClose call proceed past this block, so the
            // call-outs below run once.
            this->closed = true;
            dataChannelToClose = std::move(this->dataChannel);
            connectionToClose = std::move(this->connection);
            this->dataChannel = nullptr;
            this->connection = nullptr;
        }
        // Call-outs run OUTSIDE mMutex (phase-A0 locking policy: no
        // call-outs under a connection mutex). emit<Disconnected> reaches
        // the Endpoint / PendingConnection listeners, which take THEIR
        // mutexes; an rtc onStateChange racing on another thread holds one
        // of those and wants mMutex — emitting under mMutex is the ABBA
        // deadlock that hung the connector unit test.
        this->emit<Disconnected>(gracefulLeave, 0, reason);
        this->removeAllListeners();
        this->signallingEvents.removeAllListeners();
        // The rtc teardown runs on the shared worker pool, NOT inline:
        // doClose is reached both from the owner (main thread) and from
        // rtc's own onStateChange/onClosed callbacks, and
        // rtc::PeerConnection::close()/resetCallbacks() BLOCK until the
        // current callback returns. Called inline from an rtc callback that
        // is itself the reason we are closing, that is a self-join deadlock
        // (the unit-test hang when a peerless connection transitioned to
        // Failed on rtc's thread). The task owns the rtc shared_ptrs, so
        // they outlive this WebrtcConnection if it is destroyed first; the
        // teardown is idempotent and order-independent.
        if (dataChannelToClose || connectionToClose) {
            streamr::utils::SharedExecutors::worker().add(
                [dataChannelToClose, connectionToClose]() {
                    if (dataChannelToClose) {
                        try {
                            SLogger::trace("closing datachannel");
                            dataChannelToClose->resetCallbacks();
                            dataChannelToClose->close();
                        } catch (const std::exception& err) {
                            SLogger::trace(
                                "dc.close() errored: " +
                                std::string(err.what()));
                        }
                    }
                    if (connectionToClose) {
                        try {
                            connectionToClose->resetCallbacks();
                            connectionToClose->close();
                        } catch (const std::exception& err) {
                            SLogger::trace(
                                "conn.close() errored: " +
                                std::string(err.what()));
                        }
                    }
                });
        }
    }

    // Caller holds mMutex.
    void setupDataChannel(std::shared_ptr<rtc::DataChannel> channel) {
        auto self = this->sharedFromThis<WebrtcConnection>();
        this->dataChannel = std::move(channel);
        this->dataChannel->setBufferedAmountLowThreshold(
            this->params.bufferThresholdLow.value_or(
                defaultBufferThresholdLow));
        this->dataChannel->onOpen([self]() {
            SLogger::trace("dc.onOpened");
            self->onDataChannelOpen();
        });
        this->dataChannel->onClosed([self]() {
            SLogger::trace("dc.closed");
            self->doClose(false, "DataChannel closed");
        });
        this->dataChannel->onError([self](std::string err) {
            SLogger::error("error", {{"err", err}});
        });
        this->dataChannel->onBufferedAmountLow(
            [self]() { self->drainMessageQueue(); });
        this->dataChannel->onMessage([self](rtc::message_variant message) {
            SLogger::trace("dc.onMessage");
            if (std::holds_alternative<rtc::binary>(message)) {
                self->emit<Data>(std::get<rtc::binary>(std::move(message)));
            }
        });
    }

    void drainMessageQueue() {
        std::scoped_lock lock(this->mMutex);
        while (!this->messageQueue.empty() && this->dataChannel &&
               this->dataChannel->bufferedAmount() <
                   this->params.bufferThresholdHigh.value_or(
                       defaultBufferThresholdHigh)) {
            const auto data = std::move(this->messageQueue.front());
            this->messageQueue.pop_front();
            try {
                this->dataChannel->send(data.data(), data.size());
            } catch (const std::exception& err) {
                SLogger::debug(
                    "Failed to send binary message",
                    {{"err", std::string(err.what())}});
            }
        }
    }

    void onDataChannelOpen() {
        {
            std::scoped_lock lock(this->mMutex);
            if (this->closed) {
                return;
            }
        }
        SLogger::trace(
            "DataChannel opened for peer " +
            Identifiers::getNodeIdFromPeerDescriptor(
                this->params.remotePeerDescriptor));
        this->emit<Connected>();
    }

    void onStateChange(rtc::PeerConnection::State state) {
        SLogger::trace(
            "onStateChange " + std::to_string(static_cast<int>(state)));
        {
            std::scoped_lock lock(this->mMutex);
            this->lastState = state;
        }
        if (state == rtc::PeerConnection::State::Closed ||
            state == rtc::PeerConnection::State::Disconnected ||
            state == rtc::PeerConnection::State::Failed) {
            this->doClose(false);
        }
    }
};

} // namespace streamr::dht::connection::webrtc
