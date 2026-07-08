// Module streamr.json.toString
// CONSOLIDATED from the former header
// streamr-json/toString.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;
#include <new>


export module streamr.json.toString;

import std;

import streamr.json.jsonConcepts;
import streamr.json.toJson;

/**
 * Convert (almost) any struct to string in C++20.
 **/

export namespace streamr::json {

using suppresslint::SuppressLint; // otherwise linter thinks jsonConcepts.hpp
                                  // is unused
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