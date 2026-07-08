// Module streamr.dht.Handshaker
// CONSOLIDATED from the former header streamr-dht/connection/Handshaker.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <optional>
#include <string>
#include <tuple>

export module streamr.dht.Handshaker;

import streamr.dht.protos;

import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;
import streamr.utils.EnableSharedFromThis;
import streamr.utils.Uuid;
import streamr.dht.Connection;
import streamr.dht.Version;

// Hoisted from the former header (file scope, NOT exported);
// fully qualified: relative namespace names resolve differently
// at file scope than inside the package namespace.
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;
using streamr::utils::Uuid;
export namespace streamr::dht::connection {

using ::dht::HandshakeError;
using ::dht::HandshakeRequest;
using ::dht::HandshakeResponse;
using ::dht::Message;
using ::dht::PeerDescriptor;
using streamr::dht::connection::Connection;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::helpers::Version;
namespace handshakerevents {

struct HandshakeCompleted : Event<PeerDescriptor /*remote*/> {};
struct HandshakeFailed : Event<std::optional<HandshakeError>> {};
struct HandshakerStopped : Event<> {};
} // namespace handshakerevents

using HandshakerEvents = std::tuple<
    handshakerevents::HandshakeCompleted,
    handshakerevents::HandshakeFailed,
    handshakerevents::HandshakerStopped>;

class Handshaker : public EventEmitter<HandshakerEvents>,
                   public EnableSharedFromThis {
public:
    static constexpr auto handshakerServiceId = "system/handshaker";

    ~Handshaker() override { SLogger::debug("Handshaker::~Handshaker()"); };

    void stop() {
        auto self = this->sharedFromThis<Handshaker>();
        self->connection->off<Data>(this->onDataHandlerToken);
        self->emit<handshakerevents::HandshakerStopped>();
        self->removeAllListeners();
    }

    Message createHandshakeResponse(std::optional<HandshakeError> error) {
        HandshakeResponse outgoingHandshakeResponse;
        outgoingHandshakeResponse.mutable_sourcepeerdescriptor()->CopyFrom(
            localPeerDescriptor);
        if (error.has_value()) {
            outgoingHandshakeResponse.set_error(error.value());
        }
        outgoingHandshakeResponse.set_protocolversion(
            Version::localProtocolVersion);
        outgoingHandshakeResponse.set_applicationversion(
            Version::localApplicationVersion);

        Message message;
        message.set_serviceid(handshakerServiceId);
        message.set_messageid(Uuid::v4());
        message.mutable_handshakeresponse()->CopyFrom(
            outgoingHandshakeResponse);
        return message;
    }

private:
    eventemitter::HandlerToken onDataHandlerToken;

protected:
    const PeerDescriptor& localPeerDescriptor; // NOLINT
    std::shared_ptr<Connection> connection; // NOLINT

    Handshaker(
        const PeerDescriptor& localPeerDescriptor,
        const std::shared_ptr<Connection>& connection)
        : localPeerDescriptor(localPeerDescriptor), connection(connection) {}

    // Must be called by the newInstance() factories right after
    // construction, once the instance is owned by a shared_ptr. The
    // connection can emit from another thread concurrently with this
    // handshaker being stopped and destroyed; a `once` handler already
    // dequeued for invocation survives a concurrent off(), so the
    // handler must hold a weak reference instead of a raw `this`
    // (found by the ported SimultaneousConnections test, phase 0.3).
    void registerBaseEventHandlers() {
        std::weak_ptr<Handshaker> weakSelf = this->sharedFromThis<Handshaker>();
        this->onDataHandlerToken = this->connection->on<Data>(
            [weakSelf](const std::vector<std::byte>& data) {
                if (auto self = weakSelf.lock()) {
                    self->onData(data);
                }
            });
    }

    Message createHandshakeRequest(const PeerDescriptor& remotePeerDescriptor) {
        HandshakeRequest outgoingHandshake;
        outgoingHandshake.mutable_sourcepeerdescriptor()->CopyFrom(
            localPeerDescriptor);
        outgoingHandshake.mutable_targetpeerdescriptor()->CopyFrom(
            remotePeerDescriptor);
        outgoingHandshake.set_protocolversion(Version::localProtocolVersion);
        outgoingHandshake.set_applicationversion(
            Version::localApplicationVersion);
        Message message;
        message.set_serviceid(handshakerServiceId);
        message.set_messageid(Uuid::v4());
        message.mutable_handshakerequest()->CopyFrom(outgoingHandshake);
        return message;
    }

    void sendHandshakeRequest(const PeerDescriptor& remotePeerDescriptor) {
        const auto msg = createHandshakeRequest(remotePeerDescriptor);
        size_t nBytes = msg.ByteSizeLong();
        if (nBytes == 0) {
            SLogger::error(
                "sendHandshakeRequest(): handshake request is empty");
            return;
        }
        std::vector<std::byte> byteVec(nBytes);
        msg.SerializeToArray(byteVec.data(), static_cast<int>(nBytes));
        this->connection->send(byteVec);
        SLogger::debug(
            "sendHandshakeRequest() sending handshake request:" +
            msg.DebugString());

        SLogger::trace("sendHandshakeRequest(): handshake request sent");
    }

    void sendHandshakeResponse(
        std::optional<HandshakeError> error = std::nullopt) {
        const auto msg = createHandshakeResponse(error);
        size_t nBytes = msg.ByteSizeLong();
        if (nBytes == 0) {
            SLogger::error(
                "sendHandshakeResponse(): handshake response is empty");
            return;
        }
        std::vector<std::byte> byteVec(nBytes);
        msg.SerializeToArray(byteVec.data(), static_cast<int>(nBytes));
        this->connection->send(byteVec);
        SLogger::trace(
            "sendHandshakeResponse(): handshake response sent: " +
            msg.DebugString());
    }

    static bool isAcceptedVersion(const std::string& protocolVersion) {
        return Version::isMaybeSupportedVersion(protocolVersion);
    }

    virtual void onHandshakeRequest(
        const PeerDescriptor& source,
        const std::string& protocolVersion,
        const std::optional<PeerDescriptor>& target) = 0;
    virtual void onHandshakeResponse(const HandshakeResponse& response) = 0;
    virtual void handleSuccess(const PeerDescriptor& peerDescriptor) = 0;
    virtual void handleFailure(std::optional<HandshakeError> error) = 0;

private:
    void onData(const std::vector<std::byte>& data) {
        try {
            auto self = this->sharedFromThis<Handshaker>();
            Message message;
            message.ParseFromArray(data.data(), static_cast<int>(data.size()));
            SLogger::trace(
                "Handshaker::onData() handshake message received " +
                message.DebugString());
            if (message.has_handshakerequest()) {
                SLogger::trace(
                    "Handshaker::onData() handshake request received");
                const auto& handshake = message.handshakerequest();
                this->onHandshakeRequest(
                    handshake.sourcepeerdescriptor(),
                    handshake.protocolversion(),
                    handshake.targetpeerdescriptor());
            } else if (message.has_handshakeresponse()) {
                SLogger::trace(
                    "Handshaker::onData() handshake response received");
                this->onHandshakeResponse(message.handshakeresponse());
            } else {
                SLogger::error(
                    "Handshaker::onData() received unknown message type");
            }
        } catch (const std::exception& err) {
            SLogger::error(
                "Handshaker::onData() error while parsing handshake message: " +
                std::string(err.what()));
        }
    }
};

} // namespace streamr::dht::connection
