// Façade partition over streamr-json/jsonConcepts.hpp (see the
// streamr.eventemitter partitions for the pattern rationale). nlohmann and
// boost-pfr arrive in the global module fragment and are NOT re-exported —
// third-party libraries stay #include on the consumer side.
module;

#include "streamr-json/jsonConcepts.hpp"

export module streamr.json:jsonConcepts;

export namespace streamr::json {

using streamr::json::AssignableToNlohmannJson;
using streamr::json::AssociativeType;
using streamr::json::InitializerList;
using streamr::json::IterableType;
using streamr::json::NotAssignableToNlohmannJson;
using streamr::json::PointerLike;
using streamr::json::PointerType;
using streamr::json::ReflectableType;
using streamr::json::TypeWithToJson;

} // namespace streamr::json
