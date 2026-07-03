// Façade partition over streamr-proto-rpc/RpcCommunicatorClientApi.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-proto-rpc/RpcCommunicatorClientApi.hpp"

export module streamr.protorpc:RpcCommunicatorClientApi;

export namespace streamr::protorpc {

using streamr::protorpc::RpcCommunicatorClientApi;
using streamr::protorpc::threadPoolSize;

} // namespace streamr::protorpc
