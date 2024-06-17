#ifndef STREAMR_JSON_TOJSON_HPP
#define STREAMR_JSON_TOJSON_HPP

#include <initializer_list>
#include <string_view>
#include <type_traits>
#include <boost/pfr.hpp>
#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/pfr/traits.hpp>
#include <boost/pfr/tuple_size.hpp>
#include <nlohmann/json.hpp>
#include <streamr-json/JsonBuilder.hpp>
#include <streamr-json/jsonConcepts.hpp>

namespace streamr::json {

using json = nlohmann::json;
using JsonInitializerList = nlohmann::json::initializer_list_t;

// Forward-declaration of this template function is necessary because it is used
// in the template specialization below and calls toJson()

template <typename T>
void addStructElementToJson(json& j, std::string_view key, const T& value);

// This helper template function is needed, because you cannot use
// parameter pack expansion on an expression

template <typename StructT, std::size_t... Is>
void addStructElementsToJson(
    json& j, const StructT& st, std::index_sequence<Is...>) { // NOLINT

    (addStructElementToJson(
         j,
         boost::pfr::get_name<Is, StructT>(),
         boost::pfr::get<Is, StructT>(st)),
     ...);
}

// if none of the other templates match, use JSON builder
template <AssignableToJsonBuilder T = std::initializer_list<JsonBuilder>>
json toJson(const T& value) {
    // static_assert(!std::is_pointer_v<T>, "Pointers cannot be converted to
    // JSON");
    if constexpr (std::is_null_pointer_v<T> || std::is_pointer_v<T>) {
        return pointerToJson(value);
    }
    return JsonBuilder(value).getJson();
}

template <ReflectableType T>
json toJson(const T& value) {
    // static_assert(!std::is_pointer_v<T>, "Pointers cannot be converted to
    // JSON");
    // if constexpr (std::is_null_pointer_v<T> || std::is_pointer_v<T>) {
    //    return pointerToJson(value);
    //}
    json j;
    constexpr std::size_t structSize = boost::pfr::tuple_size<T>::value;
    addStructElementsToJson(j, value, std::make_index_sequence<structSize>{});
    if (j.is_null()) {
        j = json({});
    }
    return j;
}

template <IterableType T>
json toJson(const T& value) {
    // if constexpr (std::is_null_pointer_v<T> || std::is_pointer_v<T>) {
    //     return pointerToJson(value);
    // }
    json j;
    for (const auto& elem : value) {
        j.push_back(toJson(elem));
    }
    return j;
}
template <AssociativeType T>
json toJson(const T& value) {
    // if constexpr (std::is_null_pointer_v<T> || std::is_pointer_v<T>) {
    //     return pointerToJson(value);
    // }
    json j;
    for (const auto& elem : value) {
        j[elem.first] = toJson(elem.second);
    }
    return j;
}

// The default type parameter is needed because otherwise the template
// specialization cannot recognize the curly-bracket initializer list

template <AssignableToNlohmannJson T>
json toJson(const T& value) {
    // if constexpr (std::is_null_pointer_v<T> || std::is_pointer_v<T>) {
    //     return pointerToJson(value);
    // }
    json j;
    j = value;
    return j;
}

/*
template <NotAssignableToJson T>
json toJson(const T& value) {   // NOLINT(misc-unused-parameters)
    json j;
    j = "defaultarvo";
    return j;
}
*/

template <TypeWithToJson T>
json toJson(const T& value) {
    return value.toJson();
}

// Handling of pointer types

// template <typename T>
// bool isNullPointer(const T* value) {
//     return value == nullptr;
// }

template <typename T>
json pointerToJson(T value) {
    // if (isNullPointer(*value)) {
    if (!value) {
        return json(nullptr);
    }

    return toJson(*value);
}

template <PointerType T>
json toJson(T value) {
    return pointerToJson(value);
}

// Definition of the template function forward-declared above

template <typename T>
void addStructElementToJson(
    json& j, const std::string_view key, const T& value) {
    j[key] = toJson<T>(value);
}

} // namespace streamr::json

#endif
