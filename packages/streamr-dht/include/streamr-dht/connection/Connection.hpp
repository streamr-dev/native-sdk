#ifndef STREAMR_DHT_CONNECTION_CONNECTION_HPP
#define STREAMR_DHT_CONNECTION_CONNECTION_HPP

#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>
#include <magic_enum.hpp>
#include "streamr-eventemitter/EventEmitter.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-utils/Branded.hpp"
#include "streamr-utils/Uuid.hpp"
namespace streamr::dht::connection {

using streamr::eventemitter::Event;
using streamr::eventemitter::EventEmitter;
using streamr::logger::SLogger;
using streamr::utils::Branded;
using streamr::utils::Uuid;
using ConnectionID = Branded<std::string, struct ConnectionIDBrand>;
using Url = Branded<std::string, struct UrlBrand>;

// NOLINTBEGIN
enum class ConnectionType {
    WEBSOCKET_SERVER,
    WEBSOCKET_CLIENT,
    WEBRTC,
    SIMULATOR_SERVER,
    SIMULATOR_CLIENT
};
// NOLINTEND

// Events

namespace connectionevents {
struct Data : Event<std::vector<std::byte> /*data*/> {};
struct Connected : Event<> {};
struct Disconnected
    : Event<bool /*gracefulLeave*/, uint64_t /*code*/, std::string /*reason*/> {
};

struct Error : Event<std::string /*name*/> {};

} // namespace connectionevents

using ConnectionEvents = std::tuple<
    connectionevents::Data,
    connectionevents::Connected,
    connectionevents::Disconnected,
    connectionevents::Error>;

class Connection : public EventEmitter<ConnectionEvents> {
private:
    ConnectionID mID{std::move(createRandomConnectionId())};
    ConnectionType mType;

protected:
    explicit Connection(ConnectionType type) : mType(type) {}

public:
    virtual void send(const std::vector<std::byte>& data) = 0;
    virtual void close(bool gracefulLeave) = 0;
    virtual void destroy() = 0;

    virtual ~Connection() { SLogger::trace("~Connection()"); }

    [[nodiscard]] ConnectionID getConnectionID() const { return mID; }
    [[nodiscard]] ConnectionType getConnectionType() const { return mType; }
    [[nodiscard]] std::string getConnectionTypeString() const {
        return std::string(magic_enum::enum_name(mType));
    }
    static ConnectionID createRandomConnectionId() {
       return ConnectionID{Uuid::v4()};
    }
};

} // namespace streamr::dht::connection

#endif // STREAMR_DHT_CONNECTION_CONNECTION_HPP