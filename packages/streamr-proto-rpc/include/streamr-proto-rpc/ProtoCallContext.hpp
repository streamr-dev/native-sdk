#ifndef STREAMR_PROTO_RPC_PROTO_CALL_CONTEXT_HPP
#define STREAMR_PROTO_RPC_PROTO_CALL_CONTEXT_HPP

#include <map>
#include <string>

namespace streamr::protorpc {

using ProtoCallContext = std::map<std::string, std::string>;

} // namespace streamr::protorpc

#endif // STREAMR_PROTO_RPC_PROTO_CALL_CONTEXT_HPP