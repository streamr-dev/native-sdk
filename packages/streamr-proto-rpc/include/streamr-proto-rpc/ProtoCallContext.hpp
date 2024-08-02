#ifndef STREAMR_PROTO_RPC_PROTO_CALL_CONTEXT_HPP
#define STREAMR_PROTO_RPC_PROTO_CALL_CONTEXT_HPP

#include <map>

namespace streamr::protorpc {

struct ProtoCallContext : public std::map<std::string, std::string> {
    bool notification = false;
};

} // namespace streamr::protorpc

#endif // STREAMR_PROTO_RPC_PROTO_CALL_CONTEXT_HPP