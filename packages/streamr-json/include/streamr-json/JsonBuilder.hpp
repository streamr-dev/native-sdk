#ifndef STREAMR_JSON_JSONBUILDER_HPP
#define STREAMR_JSON_JSONBUILDER_HPP

#include <type_traits>
#include <nlohmann/json.hpp>
#include "streamr-json/jsonConcepts.hpp"

namespace streamr::json {
using json = nlohmann::json;

/**
 * @brief A private tool class for building nlohmann::json objects out of initializer lists. 
 * This class should not be used directly by the user, use toJson() function instead!
 * @details For example, the following json:
 * @code {dataId: "123", dataPoints: [1,7,3]}
 * can be expressed in the nlohmann initializer list format as
 * @code { {"dataId", "123"}, {"dataPoints", {1, 7, 3} } }
 * If this is passed to JsonBuilder constructor:
 * @code JsonBuilder j{ {"dataId", "123"}, {"dataPoints", {1, 7, 3} } };
 * The compiler will generate code that call JsonBuilder constructors recursively in a depth-first manner.
 * The JsonBuilder(const T& value) constructors are first executed on the primitive types, to cenvert them 
 * To JsonBuilders. JsonBuilder(std::initializer_list<JsonBuilder> init) 
 * constructor is then recursively called on the sections of JsonBuilders delimited by curly brackets, until the root of the treee is reached.
 * @note This class is not meant to be used directly by the user, use toJson() function instead.
 */
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

} // namespace streamr::json

#endif