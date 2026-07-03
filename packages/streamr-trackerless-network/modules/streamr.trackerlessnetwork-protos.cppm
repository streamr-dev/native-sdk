// :protos partition — wraps the GENERATED NetworkRpc protobuf trio in the
// global module fragment and re-exports the types used in this package's
// public APIs. NB: NetworkRpc.proto declares no package, so the generated
// types live in the GLOBAL namespace — hence plain `export using ::X;`.
module;

#include "packages/network/protos/NetworkRpc.client.pb.h"
#include "packages/network/protos/NetworkRpc.pb.h"
#include "packages/network/protos/NetworkRpc.server.pb.h"

export module streamr.trackerlessnetwork:protos;

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

// generated client/server stubs (the protoc plugin emits them in
// namespace streamr::protorpc)
export namespace streamr::protorpc {

using streamr::protorpc::ContentDeliveryRpc;
using streamr::protorpc::ContentDeliveryRpcClient;
using streamr::protorpc::HandshakeRpc;
using streamr::protorpc::HandshakeRpcClient;
using streamr::protorpc::NeighborUpdateRpc;
using streamr::protorpc::NeighborUpdateRpcClient;
using streamr::protorpc::NodeInfoRpc;
using streamr::protorpc::NodeInfoRpcClient;
using streamr::protorpc::ProxyConnectionRpc;
using streamr::protorpc::ProxyConnectionRpcClient;
using streamr::protorpc::TemporaryConnectionRpc;
using streamr::protorpc::TemporaryConnectionRpcClient;

} // namespace streamr::protorpc
