// Module streamr.dht.DhtCallContext
// CONSOLIDATED from the former header
// streamr-dht/rpc-protocol/DhtCallContext.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;
#include <new>


export module streamr.dht.DhtCallContext;

import std;

import streamr.dht.protos;
export namespace streamr::dht::rpcprotocol {

using ::dht::PeerDescriptor;

struct DhtCallContext {
    // used by client
    std::optional<PeerDescriptor> targetDescriptor;
    std::optional<PeerDescriptor> sourceDescriptor;
    std::optional<std::uint64_t> clientId;
    std::optional<bool> connect;
    std::optional<bool> sendIfStopped;
    // used in incoming calls
    std::optional<PeerDescriptor> incomingSourceDescriptor;
};
} // namespace streamr::dht::rpcprotocol
