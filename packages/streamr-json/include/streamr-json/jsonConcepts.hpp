#ifndef STREAMR_JSON_JSONCONCEPTS_HPP
#define STREAMR_JSON_JSONCONCEPTS_HPP

#include <ranges>
#include <boost/pfr/traits.hpp>
#include <nlohmann/json.hpp>

namespace streamr::json {

template <typename Ptr>
concept PointerLike = std::is_pointer_v<Ptr> || requires(Ptr p) {
    { *p };
    { static_cast<bool>(p) };
    { p.operator->() } -> std::convertible_to<decltype(&*p)>;
};

template <typename T>
concept PointerType = std::is_null_pointer_v<T> || PointerLike<T>;

template <typename T>
concept AssignableToNlohmannJson =
    std::is_assignable<nlohmann::json&, T>::value;

template <typename T>
concept NotAssignableToNlohmannJson =
    !std::is_assignable<nlohmann::json&, T>::value;

template <typename T>
concept TypeWithToJson = requires(T obj) {
    { obj.toJson() } -> std::same_as<nlohmann::json>;
};

template <typename Container>
concept AssociativeType = requires(Container cont) {
    typename Container::key_type;
    typename Container::mapped_type;
    { cont.begin() } -> std::same_as<typename Container::iterator>;
    { cont.end() } -> std::same_as<typename Container::iterator>;
    requires(not AssignableToNlohmannJson<Container>);
    requires(not PointerType<Container>);
};

template <typename T>
concept IterableType = requires(T container) {
    requires std::ranges::range<T>;
    requires(not std::is_same<T, std::string>::value);
    requires(not AssociativeType<T>);
    requires(not AssignableToNlohmannJson<T>);
    requires(not PointerType<T>);
};

template <typename T>
concept InitializerList = std::is_same<T, std::initializer_list<T>>::value;

// template <typename T>
// concept NotAssignableToJson = !std::is_assignable<nlohmann::json&, T>::value;

template <typename T>
concept ReflectableType = requires(T value) {
    { boost::pfr::is_implicitly_reflectable<T, std::nullptr_t>::value };
    requires(not AssociativeType<T>);
    requires(not IterableType<T>);
    requires(not TypeWithToJson<T>);
    requires(not AssignableToNlohmannJson<T>);
    requires(not PointerType<T>);
};

/*
template <typename T>
concept OtherInitializerList = requires(T obj) {
    requires(not AssociativeType<T>);
    requires(not IterableType<T>);
    requires (not TypeWithToJson<T>);
    requires (not AssignableToNlohmannJson<T>);
    requires (noaland::is_fuzzy_type_matched_v<T,
std::initializer_list<noaland::i_dont_care>>);
};
*/

} // namespace streamr::json

#endif // STREAMR_JSON_JSONCONCEPTS_HPP