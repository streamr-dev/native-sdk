#ifndef STREAMR_DHT_DHT_CALL_CONTEXT_HPP
#define STREAMR_DHT_DHT_CALL_CONTEXT_HPP

#include <optional>
#include "packages/dht/protos/DhtRpc.pb.h"
#include "streamr-proto-rpc/ProtoCallContext.hpp"

namespace streamr::dht::rpcprotocol {

using ::dht::PeerDescriptor;
using streamr::protorpc::ProtoCallContext;

struct DhtCallContext : public ProtoCallContext {
    // used by client
    std::optional<PeerDescriptor> targetDescriptor;
    std::optional<PeerDescriptor> sourceDescriptor;
    std::optional<uint64_t> clientId;
    std::optional<bool> connect;
    std::optional<bool> sendIfStopped;
    // used in incoming calls
    std::optional<PeerDescriptor> incomingSourceDescriptor;
};
} // namespace streamr::dht::rpcprotocol

#endif