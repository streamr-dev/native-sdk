// Façade partition over streamr-json/toJson.hpp. `using
// streamr::json::toJson;` re-exports the whole overload set (all concept
//-constrained specializations).
module;

#include "streamr-json/toJson.hpp"

export module streamr.json:toJson;

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
