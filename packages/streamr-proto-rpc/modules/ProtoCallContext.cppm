// Module streamr.protorpc.ProtoCallContext
// CONSOLIDATED from the former header streamr-proto-rpc/ProtoCallContext.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;
#include <new>


export module streamr.protorpc.ProtoCallContext;

import std;
export namespace streamr::protorpc {

using ProtoCallContext = std::map<std::string, std::string>;

} // namespace streamr::protorpc
