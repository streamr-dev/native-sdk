#ifndef STREAMR_JSON_TOSTRING_HPP
#define STREAMR_JSON_TOSTRING_HPP

/**
 * Convert (almost) any struct to string in C++20.
 **/

#include "streamr-json/JsonBuilder.hpp"
#include "streamr-json/jsonConcepts.hpp"
#include "streamr-json/toJson.hpp"

namespace streamr::json {

template <typename T>
concept TypeWithToString = requires(T obj) {
    { obj.toString() } -> std::same_as<std::string>;
};

template <typename T>
std::string toString(const T& value) {
    return toJson<T>(value).dump();
}

template <AssignableToJsonBuilder T = std::initializer_list<JsonBuilder>>
std::string toString(const T& value) {
    return toJson<T>(value).dump();
}

template <InitializerList T>
std::string toString(const T& value) {
    return toJson<T>(value).dump();
}

template <AssignableToNlohmannJson T>
std::string toString(const T& value) {
    return toJson<T>(value).dump();
}

template <TypeWithToString T>
std::string toString(const T& value) {
    return value.toString();
}

} // namespace streamr::json

#endif
