// :protos partition — wraps the GENERATED DhtRpc protobuf trio (messages,
// enums, and the protoc-plugin-generated client/server stubs) in the
// global module fragment and re-exports the types used across this
// package's public APIs. Generated protobuf code stays #include-based
// permanently; this partition is how importers see the types without
// re-parsing the 12k-line header stack.
module;

#include "packages/dht/protos/DhtRpc.client.pb.h"
#include "packages/dht/protos/DhtRpc.pb.h"
#include "packages/dht/protos/DhtRpc.server.pb.h"

export module streamr.dht.protos;

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

// generated client stubs
using ::dht::ConnectionLockRpcClient;
using ::dht::DhtNodeRpcClient;
using ::dht::ExternalApiRpcClient;
using ::dht::RecursiveOperationRpcClient;
using ::dht::RecursiveOperationSessionRpcClient;
using ::dht::RouterRpcClient;
using ::dht::StoreRpcClient;
using ::dht::WebrtcConnectorRpcClient;
using ::dht::WebsocketClientConnectorRpcClient;

// generated server stubs
using ::dht::ConnectionLockRpc;
using ::dht::DhtNodeRpc;
using ::dht::ExternalApiRpc;
using ::dht::RecursiveOperationRpc;
using ::dht::RecursiveOperationSessionRpc;
using ::dht::RouterRpc;
using ::dht::StoreRpc;
using ::dht::WebrtcConnectorRpc;
using ::dht::WebsocketClientConnectorRpc;

} // namespace dht
