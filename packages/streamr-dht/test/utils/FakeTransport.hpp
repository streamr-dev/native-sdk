#ifndef STREAMR_DHT_TEST_UTILS_FAKE_TRANSPORT_HPP
#define STREAMR_DHT_TEST_UTILS_FAKE_TRANSPORT_HPP
#include <functional>
#include <map>
#include <memory>
#include <ranges>
#include "streamr-dht/connection/ConnectionsView.hpp"
#include "streamr-dht/Identifiers.hpp"
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-dht/transport/Transport.hpp"

namespace streamr::dht::test::utils {

using ::dht::Message;
using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::transport::SendOptions;
using streamr::dht::transport::Transport;

class FakeTransport : public Transport, public ConnectionsView {
private:
    std::function<void(const Message&)> onSend;
    const PeerDescriptor& localPeerDescriptor;
    // currently adds a peerDescription to the connections array when a
    // "connect" option is seen in in send() call and never disconnects (TODO
    // could add some disconnection logic? and maybe the connection should be
    // seen by another FakeTransport instance, too?)
    std::map<DhtAddress, PeerDescriptor> connections;

public:
    explicit FakeTransport(
        const PeerDescriptor& peerDescriptor,
        const std::function<void(const Message&)>& onSend)
        : localPeerDescriptor(peerDescriptor), onSend(onSend) {}

    void send(const Message& msg, const SendOptions& opts) override {
        Message newMsg = msg;

        const auto connect = opts.connect;
        const auto targetNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(msg.targetdescriptor());

        if (connect && !this->connections.contains(targetNodeId)) {
            this->connect(msg.targetdescriptor());
        }

        newMsg.mutable_sourcedescriptor()->CopyFrom(this->localPeerDescriptor);
        this->onSend(newMsg);
    }

    [[nodiscard]] PeerDescriptor getLocalPeerDescriptor() const override {
        return this->localPeerDescriptor;
    }

    [[nodiscard]] std::vector<PeerDescriptor> getConnections() const override {
        return this->connections | std::views::values |
            std::ranges::to<std::vector>();
    }

    [[nodiscard]] size_t getConnectionCount() const override {
        return this->connections.size();
    }

    [[nodiscard]] bool hasConnection(const DhtAddress& nodeId) const override {
        return this->connections.contains(nodeId);
    }

    void stop() override {}

private:
    void connect(const PeerDescriptor& peerDescriptor) {
        this->connections.emplace(
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor),
            peerDescriptor);
    }
};

class FakeEnvironment {
private:
    std::map<DhtAddress, std::shared_ptr<FakeTransport>> transports;

public:
    [[nodiscard]] std::shared_ptr<FakeTransport> createTransport(
        const PeerDescriptor& peerDescriptor) {
        
        auto transport = std::make_shared<FakeTransport>(
            peerDescriptor, [this](const Message& msg) {
                const auto targetNodeId = Identifiers::getNodeIdFromPeerDescriptor(
                    msg.targetdescriptor());

                const auto targetTransport = this->transports.find(targetNodeId);
                if (targetTransport != this->transports.end()) {
                    targetTransport->second->emit<transport::transportevents::Message>(msg);
                }
            });

        this->transports.emplace(
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor),
            transport);
        
        return transport;
    }
};

} // namespace streamr::dht::test::utils

#endif // STREAMR_DHT_TEST_UTILS_FAKE_TRANSPORT_HPP
