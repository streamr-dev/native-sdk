// Façade partition over streamr-proto-rpc/Errors.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-proto-rpc/Errors.hpp"

export module streamr.protorpc:Errors;

export namespace streamr::protorpc {

using streamr::protorpc::Err;
using streamr::protorpc::ErrorCode;
using streamr::protorpc::FailedToParse;
using streamr::protorpc::FailedToSerialize;
using streamr::protorpc::RpcClientError;
using streamr::protorpc::RpcException;
using streamr::protorpc::RpcRequestError;
using streamr::protorpc::RpcServerError;
using streamr::protorpc::RpcTimeout;
using streamr::protorpc::UnknownRpcMethod;

} // namespace streamr::protorpc
