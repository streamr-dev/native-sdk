#ifndef STREAMR_DHT_CONNECTION_HANDSHAKER_HPP
#define STREAMR_DHT_CONNECTION_HANDSHAKER_HPP

#include <optional>
#include <string>
#include <tuple>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/helpers/Version.hpp"
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/EnableSharedFromThis.hpp"
#include "streamr-utils/Uuid.hpp"

namespace streamr::dht::connection {

using ::dht::HandshakeError;
using ::dht::HandshakeRequest;
using ::dht::HandshakeResponse;
using ::dht::Message;
using ::dht::PeerDescriptor;
using streamr::dht::connection::Connection;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::helpers::Version;
using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::EnableSharedFromThis;
using streamr::utils::Uuid;
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
        outgoingHandshakeResponse.set_version(Version::localProtocolVersion);

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
        : localPeerDescriptor(localPeerDescriptor), connection(connection) {
        this->onDataHandlerToken = this->connection->on<Data>(
            [this](const std::vector<std::byte>& data) { this->onData(data); });
    }

    Message createHandshakeRequest(const PeerDescriptor& remotePeerDescriptor) {
        HandshakeRequest outgoingHandshake;
        outgoingHandshake.mutable_sourcepeerdescriptor()->CopyFrom(
            localPeerDescriptor);
        outgoingHandshake.mutable_targetpeerdescriptor()->CopyFrom(
            remotePeerDescriptor);
        outgoingHandshake.set_version(Version::localProtocolVersion);
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

    static bool isAcceptedVersion(const std::string& version) {
        return Version::isMaybeSupportedVersion(version);
    }

    virtual void onHandshakeRequest(
        const PeerDescriptor& source,
        const std::string& version,
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
            const auto debugString = message.DebugString();
            SLogger::trace(
                "Handshaker::onData() handshake message received " +
                message.DebugString());
            if (message.has_handshakerequest()) {
                SLogger::trace(
                    "Handshaker::onData() handshake request received");
                const auto& handshake = message.handshakerequest();
                this->onHandshakeRequest(
                    handshake.sourcepeerdescriptor(),
                    handshake.version(),
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

#endif // STREAMR_DHT_CONNECTION_HANDSHAKER_HPP
