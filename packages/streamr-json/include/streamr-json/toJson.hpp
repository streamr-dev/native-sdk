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

/**
 * @brief Specialization to initializer_lists. The initializer lists should be
 * in the nlohmann::json format.
 * @tparam T The type of initializer list to convert to JSON.
 * @param value The initializer list to convert to JSON. The list can hold
 * almost any types, except the ones with private members.
 * @return The JSON representation of the value.
 */

template <AssignableToJsonBuilder T = std::initializer_list<JsonBuilder>>
json toJson(const T& value) {
    if constexpr (std::is_null_pointer_v<T> || std::is_pointer_v<T>) {
        return pointerToJson(value);
    }
    return JsonBuilder(value).getJson();
}

/**
 * @brief Specialization for types that are reflectable by boost::pfr. This
 * includes almost all structs that do not have private members.
 * @tparam T The type of the value to convert to JSON. T must not have private
 * members.
 * @param value The value to convert to JSON.
 * @return The JSON representation of the value.
 */

template <ReflectableType T>
json toJson(const T& value) {
    json j;
    constexpr std::size_t structSize = boost::pfr::tuple_size<T>::value;
    addStructElementsToJson(j, value, std::make_index_sequence<structSize>{});
    if (j.is_null()) {
        j = json({});
    }
    return j;
}

/**
 * @brief Specialization for iterable types such as std::vector, std::list, etc.
 * @tparam T The type of the value to convert to JSON. T must be an iterable
 * type.
 * @param value The value to convert to JSON.
 * @return The JSON representation of the value.
 */

template <IterableType T>
json toJson(const T& value) {
    json j;
    for (const auto& elem : value) {
        j.push_back(toJson(elem));
    }
    return j;
}

/**
 * @brief Specialization for associative types such as std::map,
 * std::unordered_map, etc.
 * @tparam T The type of the value to convert to JSON. T must be an associative
 * type.
 * @param value The value to convert to JSON.
 * @return The JSON representation of the value.
 */

template <AssociativeType T>
json toJson(const T& value) {
    json j;
    for (const auto& elem : value) {
        j[elem.first] = toJson(elem.second);
    }
    return j;
}

/**
 * @brief Specialization for types that are directly assignable to
 * nlohmann::json.
 * @tparam T The type of the value to convert to JSON. T must be assignable to
 * nlohmann::json.
 * @param value The value to convert to JSON.
 * @return The JSON representation of the value.
 */

template <AssignableToNlohmannJson T>
json toJson(const T& value) {
    json j;
    j = value;
    return j;
}

/**
 * @brief Specialization for types that have a toJson() member function.
 * @tparam T The type of the value to convert to JSON. T must have a member
 * function with the signature nlohmann::json toJson() const;
 * @param value The value to convert to JSON.
 * @return The JSON representation of the value.
 */

template <TypeWithToJson T>
json toJson(const T& value) {
    return value.toJson();
}

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
