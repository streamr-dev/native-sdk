#ifndef STREAMR_JSON_TOJSON_HPP
#define STREAMR_JSON_TOJSON_HPP

#include <ranges>
#include <string_view>
#include <type_traits>
#include <boost/pfr.hpp>
#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <boost/pfr/traits.hpp>
#include <boost/pfr/tuple_size.hpp>
#include <nlohmann/json.hpp>

namespace streamr::json {

using json = nlohmann::json;
using JsonInitializerList = nlohmann::json::initializer_list_t;

// Concepts for template specialization

template <typename T>
concept AssignableToJson = std::is_assignable<json&, T>::value;

template <typename T>
concept TypeWithToJson = requires(T obj) {
    { obj.toJson() } -> std::same_as<json>;
};

template <typename Container>
concept AssociativeType = requires(Container cont) {
    typename Container::key_type;
    typename Container::mapped_type;
    { cont.begin() } -> std::same_as<typename Container::iterator>;
    { cont.end() } -> std::same_as<typename Container::iterator>;
    requires(not AssignableToJson<Container>);
};

template <typename T>
concept IterableType = requires(T container) {
    requires std::ranges::range<T>;
    requires(not std::is_same<T, std::string>::value);
    requires(not AssociativeType<T>);
    requires(not AssignableToJson<T>);
};

template <typename T>
concept ReflectableType = requires(T value) {
    { boost::pfr::is_implicitly_reflectable<T, std::nullptr_t>::value };
    requires(not AssociativeType<T>);
    requires(not IterableType<T>);
    requires(not TypeWithToJson<T>);
    requires(not AssignableToJson<T>);
};

/*
template <typename T>
json toJson(const T& value) { // NOLINT(misc-unused-parameters)
    static_assert(
        !ReflectableType<T>,
        "The type cannot be converted to JSON/string. For classes with \
        private fields, you MUST implement toJson()/toString() member functions
\
        in order to be able to call
toJson(classInstance)/toString(classInstance). \ Otherwise calling the functions
will result in this compile error.");

    json j = "The type cannot be converted to JSON or string: " +
        std::string(typeid(T).name());

    std::cerr << j << std::endl;

    return j;
}
*/
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

template <ReflectableType T>
json toJson(const T& value) {
    json j;
    constexpr std::size_t structSize = boost::pfr::tuple_size<T>::value;
    addStructElementsToJson(j, value, std::make_index_sequence<structSize>{});
    return j;
}

template <IterableType T>
json toJson(const T& value) {
    json j;
    for (const auto& elem : value) {
        j.push_back(toJson(elem));
    }
    return j;
}

template <AssociativeType T>
json toJson(const T& value) {
    json j;
    for (const auto& elem : value) {
        j[elem.first] = toJson(elem.second);
    }
    return j;
}

// The default type parameter is needed because otherwise the template
// specialization cannot recognize the curly-bracket initializer list

template <AssignableToJson T = JsonInitializerList>
json toJson(const T& value) {
    json j;
    j = value;
    return j;
}

template <TypeWithToJson T>
json toJson(const T& value) {
    return value.toJson();
}

// Definition of the template function forward-declared above

template <typename T>
void addStructElementToJson(
    json& j, const std::string_view key, const T& value) {
    j[key] = toJson<T>(value);
}

} // namespace streamr::json

#endif
