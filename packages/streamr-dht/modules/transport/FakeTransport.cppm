// Module streamr.dht.FakeTransport
// CONSOLIDATED from the former header streamr-dht/transport/FakeTransport.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <functional>
#include <map>
#include <memory>
#include <ranges>

#include <string>

export module streamr.dht.FakeTransport;

import streamr.dht.protos;

import streamr.dht.ConnectionsView;
import streamr.dht.Identifiers;
import streamr.dht.Transport;
export namespace streamr::dht::transport {

using ::dht::Message;
using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectionsView;
using streamr::dht::transport::SendOptions;
using streamr::dht::transport::Transport;

class FakeTransport : public Transport, public ConnectionsView {
private:
    std::function<void(const Message&)> onSend;
    // By VALUE: a reference member dangled when callers passed a temporary
    // descriptor into FakeEnvironment::createTransport (BUS error in send()
    // the first time the source descriptor was stamped).
    const PeerDescriptor localPeerDescriptor;
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

    [[nodiscard]] std::vector<PeerDescriptor> getConnections() override {
        return this->connections | std::views::values |
            std::ranges::to<std::vector>();
    }

    [[nodiscard]] size_t getConnectionCount() override {
        return this->connections.size();
    }

    [[nodiscard]] bool hasConnection(const DhtAddress& nodeId) override {
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
                const auto targetNodeId =
                    Identifiers::getNodeIdFromPeerDescriptor(
                        msg.targetdescriptor());

                const auto targetTransport =
                    this->transports.find(targetNodeId);
                if (targetTransport != this->transports.end()) {
                    targetTransport->second
                        ->emit<transport::transportevents::Message>(msg);
                }
            });

        this->transports.emplace(
            Identifiers::getNodeIdFromPeerDescriptor(peerDescriptor),
            transport);

        return transport;
    }
};

} // namespace streamr::dht::transport
