// :protos partition — wraps the GENERATED protobuf header in the global
// module fragment and re-exports the message/enum types that appear in
// this package's public APIs. Generated protobuf code stays #include-based
// permanently (protoc emits headers); this partition is how importers see
// the types without re-parsing the .pb.h.
module;

#include "packages/proto-rpc/protos/ProtoRpc.pb.h"

export module streamr.protorpc.protos;

export namespace protorpc {

using ::protorpc::RpcErrorType;
using ::protorpc::RpcMessage;
using enum ::protorpc::RpcErrorType;

} // namespace protorpc
