// Wraps the GENERATED NetworkRpc protobuf messages and enums in the
// global module fragment and re-exports the types used in this package's
// public APIs. NB: NetworkRpc.proto declares no package, so the generated
// types live in the GLOBAL namespace — hence plain `export using ::X;`.
// The RPC client/server stubs are no longer here: the protoc plugin now
// emits them as their own module units
// (streamr.trackerlessnetwork.NetworkRpcClient / ...NetworkRpcServer
// under modules/gen/) — import those directly where the stubs are used.
module;

#include "packages/network/protos/NetworkRpc.pb.h"

export module streamr.trackerlessnetwork.protos;

// messages
export using ::CloseTemporaryConnection;
export using ::ContentMessage;
export using ::ControlLayerInfo;
export using ::GroupKey;
export using ::GroupKeyRequest;
export using ::GroupKeyResponse;
export using ::InterleaveRequest;
export using ::InterleaveResponse;
export using ::LeaveStreamPartNotice;
export using ::MessageID;
export using ::MessageRef;
export using ::NeighborUpdate;
export using ::NodeInfoRequest;
export using ::NodeInfoResponse;
export using ::ProxyConnectionRequest;
export using ::ProxyConnectionResponse;
export using ::StreamMessage;
export using ::StreamPartHandshakeRequest;
export using ::StreamPartHandshakeResponse;
export using ::StreamPartitionInfo;
export using ::TemporaryConnectionRequest;
export using ::TemporaryConnectionResponse;

// enums (unscoped, global namespace; enumerators reachable via EnumName::)
export using ::ContentType;
export using ::EncryptionType;
export using ::ProxyDirection;
export using ::SignatureType;

// Ordering for MessageRef (used as an ordered-container key across the
// package). Exported here, next to the type's re-export, so every
// importer of the protos module can order MessageRef values; the
// operator lives in the global namespace like the generated type
// itself, so argument-dependent lookup finds it.
export inline bool operator<(const MessageRef& r1, const MessageRef& r2) {
    if (r1.sequencenumber() != r2.sequencenumber()) {
        return r1.sequencenumber() < r2.sequencenumber();
    }
    return r1.timestamp() < r2.timestamp();
}
