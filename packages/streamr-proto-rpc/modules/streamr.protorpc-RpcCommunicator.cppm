// Façade partition over streamr-proto-rpc/RpcCommunicator.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-proto-rpc/RpcCommunicator.hpp"

export module streamr.protorpc:RpcCommunicator;

export namespace streamr::protorpc {

using streamr::protorpc::defaultRpcRequestTimeout;
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::RpcCommunicatorOptions;
using streamr::protorpc::RpcErrorType;
using streamr::protorpc::RpcMessage;
using streamr::protorpc::StatusCode;

} // namespace streamr::protorpc
