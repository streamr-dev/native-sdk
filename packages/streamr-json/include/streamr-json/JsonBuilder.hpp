#ifndef STREAMR_JSON_JSONBUILDER_HPP
#define STREAMR_JSON_JSONBUILDER_HPP

#include <type_traits>
#include <nlohmann/json.hpp>
#include "streamr-json/jsonConcepts.hpp"

namespace streamr::json {
using json = nlohmann::json;

class JsonBuilder {
    json jsonData;

public:
    template <AssignableToNlohmannJson T>
    JsonBuilder(const T& value) { // NOLINT(google-explicit-constructor) - Allow
                                  // implicit conversion
        jsonData = value;
    }

    template <NotAssignableToNlohmannJson T>
    JsonBuilder(const T& value) { // NOLINT(google-explicit-constructor) - Allow
                                  // implicit conversion
        // static_assert(!std::is_pointer_v<T>, "Pointers cannot be converted to
        // JSON");
        /*
        if constexpr (std::is_pointer_v<T>) {
            if (&value == 0) {
                jsonData = nullptr;
            }
            else {
                jsonData = "<pointer>";
            }
        }
        */
        jsonData = toJson(value);
    }

    JsonBuilder(std::initializer_list<JsonBuilder> init) {
        bool isObject = std::all_of(
            init.begin(), init.end(), [](const JsonBuilder& element) -> bool {
                // the use of cast exists to avoid a warning on Windows
                // (according to nlohmann)
                return element.isArray() && element.getSize() == 2 &&
                    element[static_cast<size_t>(0)].is_string();
            });

        if (isObject) {
            // the initializer list is a list of pairs -> create object
            for (const auto& element : init) {
                jsonData.emplace(element[0], element[1]);
            }
        } else {
            // the initializer list describes an array -> create array
            for (const auto& element : init) {
                jsonData.push_back(element.getJson());
            }
        }
    }

    JsonBuilder(const JsonBuilder& other) = default;
    JsonBuilder(JsonBuilder&& other) noexcept = default;
    JsonBuilder& operator=(const JsonBuilder& other) = default;
    JsonBuilder& operator=(JsonBuilder&& other) noexcept = default;

    [[nodiscard]] json getJson() const { return jsonData; }

    [[nodiscard]] const json& operator[](size_t index) const {
        return jsonData[index];
    }

    [[nodiscard]] bool isObject() const { return jsonData.is_object(); }

    [[nodiscard]] bool isArray() const { return jsonData.is_array(); }

    [[nodiscard]] bool isString(size_t index) {
        return jsonData[index].is_string();
    }

    [[nodiscard]] size_t getSize() const { return jsonData.size(); }
};

template <typename T>
concept AssignableToJsonBuilder =
    std::is_same<std::initializer_list<JsonBuilder>, T>::value;
/*
requires(T value) {
    { std::is_assignable<JsonBuilder&, T>::value };
    //requires(not AssociativeType<T>);
    //requires(not IterableType<T>);
    requires(not TypeWithToJson<T>);
    requires(not AssignableToNlohmannJson<T>);
};*/

} // namespace streamr::json

#endif