// Coarse façade partition over ALL public headers of streamr-json.
// One partition (instead of one per header) keeps the number of
// module units — and thus repeated global-module-fragment parses
// of the expensive header stacks — minimal during the façade
// stage (measured at the Phase 2.4 bench checkpoint). Per-header
// granularity returns at consolidation if needed.
module;

#include "streamr-json/jsonConcepts.hpp"
#include "streamr-json/toJson.hpp"
#include "streamr-json/toString.hpp"

export module streamr.json:all;

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

export namespace streamr::json {

using streamr::json::addStructElementsToJson;
using streamr::json::addStructElementToJson;
using streamr::json::AssignableToJsonBuilder;
using streamr::json::json;
using streamr::json::JsonBuilder;
using streamr::json::JsonInitializerList;
using streamr::json::pointerToJson;
using streamr::json::StreamrJsonInitializerList;
using streamr::json::toJson;

} // namespace streamr::json

export namespace streamr::json {

using streamr::json::toString;
using streamr::json::TypeWithToString;

} // namespace streamr::json
