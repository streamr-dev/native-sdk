// Coarse façade partition over ALL public headers of streamr-proto-rpc.
// One partition (instead of one per header) keeps the number of
// module units — and thus repeated global-module-fragment parses
// of the expensive header stacks — minimal during the façade
// stage (measured at the Phase 2.4 bench checkpoint). Per-header
// granularity returns at consolidation if needed.
module;

#include "streamr-proto-rpc/Errors.hpp"
#include "streamr-proto-rpc/ProtoCallContext.hpp"
#include "streamr-proto-rpc/RpcCommunicator.hpp"
#include "streamr-proto-rpc/RpcCommunicatorClientApi.hpp"
#include "streamr-proto-rpc/RpcCommunicatorServerApi.hpp"
#include "streamr-proto-rpc/ServerRegistry.hpp"
#include "streamr-proto-rpc/StreamPrinter.hpp"

export module streamr.protorpc:all;

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

export namespace streamr::protorpc {

using streamr::protorpc::ProtoCallContext;

} // namespace streamr::protorpc

export namespace streamr::protorpc {

using streamr::protorpc::defaultRpcRequestTimeout;
using streamr::protorpc::RpcCommunicator;
using streamr::protorpc::RpcCommunicatorOptions;
using streamr::protorpc::RpcErrorType;
using streamr::protorpc::RpcMessage;
using streamr::protorpc::StatusCode;

} // namespace streamr::protorpc

export namespace streamr::protorpc {

using streamr::protorpc::RpcCommunicatorClientApi;
using streamr::protorpc::threadPoolSize;

} // namespace streamr::protorpc

export namespace streamr::protorpc {

using streamr::protorpc::RpcCommunicatorServerApi;

} // namespace streamr::protorpc

export namespace streamr::protorpc {

using streamr::protorpc::Any;
using streamr::protorpc::Empty;
using streamr::protorpc::MethodOptions;
using streamr::protorpc::ServerRegistry;

} // namespace streamr::protorpc

export namespace streamr::protorpc {

using streamr::protorpc::StreamPrinter;

} // namespace streamr::protorpc
