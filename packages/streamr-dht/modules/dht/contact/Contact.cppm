// Module streamr.dht.Contact
// Ported from packages/dht/src/dht/contact/Contact.ts (v103.8.0-rc.3).
module;

#include <utility>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.Contact;

import streamr.dht.Identifiers;

export namespace streamr::dht::contact {

using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;

// A thin wrapper that gives a PeerDescriptor the getNodeId()/
// getPeerDescriptor() shape the contact-list templates expect.
class Contact {
private:
    PeerDescriptor peerDescriptor;

public:
    explicit Contact(PeerDescriptor peerDescriptor)
        : peerDescriptor(std::move(peerDescriptor)) {}

    [[nodiscard]] PeerDescriptor getPeerDescriptor() const {
        return this->peerDescriptor;
    }

    [[nodiscard]] DhtAddress getNodeId() const {
        return Identifiers::getNodeIdFromPeerDescriptor(this->peerDescriptor);
    }
};

} // namespace streamr::dht::contact
