// Façade partition over streamr-proto-rpc/ServerRegistry.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-proto-rpc/ServerRegistry.hpp"

export module streamr.protorpc:ServerRegistry;

export namespace streamr::protorpc {

using streamr::protorpc::Any;
using streamr::protorpc::Empty;
using streamr::protorpc::MethodOptions;
using streamr::protorpc::ServerRegistry;

} // namespace streamr::protorpc
