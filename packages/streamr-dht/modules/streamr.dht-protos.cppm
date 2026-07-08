// Wraps the GENERATED DhtRpc protobuf messages and enums in the global
// module fragment and re-exports the types used across this package's
// public APIs. Generated protobuf MESSAGE code stays #include-based
// permanently; this module is how importers see the types without
// re-parsing the 12k-line header stack. The RPC client/server stubs are
// no longer here: the protoc plugin now emits them as their own module
// units (streamr.dht.DhtRpcClient / streamr.dht.DhtRpcServer under
// modules/gen/) — import those directly where the stubs are used.
module;

#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.protos;

// google.protobuf well-known types reached transitively through DhtRpc.pb.h
// (DataEntry carries an Any; timestamps use Timestamp). Re-exported so the
// modules that previously saw them via the textual include still can.
export namespace google::protobuf {
using ::google::protobuf::Any;
using ::google::protobuf::Timestamp;
} // namespace google::protobuf

export namespace dht {

// messages
using ::dht::ClosestPeersRequest;
using ::dht::ClosestPeersResponse;
using ::dht::ClosestRingPeersRequest;
using ::dht::ClosestRingPeersResponse;
using ::dht::ConnectivityMethod;
using ::dht::ConnectivityRequest;
using ::dht::ConnectivityResponse;
using ::dht::DataEntry;
using ::dht::DisconnectNotice;
using ::dht::ExternalFetchDataRequest;
using ::dht::ExternalFetchDataResponse;
using ::dht::ExternalFindClosestNodesRequest;
using ::dht::ExternalFindClosestNodesResponse;
using ::dht::ExternalStoreDataRequest;
using ::dht::ExternalStoreDataResponse;
using ::dht::HandshakeRequest;
using ::dht::HandshakeResponse;
using ::dht::IceCandidate;
using ::dht::LeaveNotice;
using ::dht::LockRequest;
using ::dht::LockResponse;
using ::dht::Message;
using ::dht::PeerDescriptor;
using ::dht::PingRequest;
using ::dht::PingResponse;
using ::dht::RecursiveOperationRequest;
using ::dht::RecursiveOperationResponse;
using ::dht::ReplicateDataRequest;
using ::dht::RouteMessageAck;
using ::dht::RouteMessageWrapper;
using ::dht::RtcAnswer;
using ::dht::RtcOffer;
using ::dht::SetPrivateRequest;
using ::dht::StoreDataRequest;
using ::dht::StoreDataResponse;
using ::dht::UnlockRequest;
using ::dht::WebrtcConnectionRequest;
using ::dht::WebsocketConnectionRequest;

// enums (unscoped in the generated code: using enum brings the enumerators)
using ::dht::DisconnectMode;
using enum ::dht::DisconnectMode;
using ::dht::HandshakeError;
using enum ::dht::HandshakeError;
using ::dht::NodeType;
using enum ::dht::NodeType;
using ::dht::RecursiveOperation;
using enum ::dht::RecursiveOperation;
using ::dht::RouteMessageError;
using enum ::dht::RouteMessageError;
using ::dht::RpcResponseError;
using enum ::dht::RpcResponseError;

} // namespace dht
